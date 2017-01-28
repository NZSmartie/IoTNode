#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "string.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "lwip/api.h"

#include "tcpip_adapter.h"

/* Server cert & key */
extern const char *server_cert;
extern const char *server_key;

/* mbedtls/config.h MUST appear before all other mbedtls headers, or
   you'll get the default config.
   (Although mostly that isn't a big problem, you just might get
   errors at link time if functions don't exist.) */
#include "mbedtls/esp_config.h"

#include "mbedtls/net.h"
#include "mbedtls/debug.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/certs.h"

#include "mbedtls/timing.h"
#include "mbedtls/ssl_cookie.h"

#include "esp_log.h"

#include "coap.h"
#include "interfaces/mbed_timer.h"

static const char *TAG = "CoAP";
static EventGroupHandle_t wifi_events;

const int COAP_CONNECTED_BIT = ( 1 << 0 );

const int OPENSSL_DEMO_RECV_BUF_LEN = 1024;
const char *LOCAL_PORT = "4433";

extern const unsigned char iotnode_crt[] asm("_binary_iotnode_crt_start");
extern const unsigned char iotnode_crt_end[]   asm("_binary_iotnode_crt_end");

extern const unsigned char iotnode_key[] asm("_binary_iotnode_key_start");
extern const unsigned char iotnode_key_end[]   asm("_binary_iotnode_key_end");

#define OPENSSL_DEMO_SERVER_ACK "HTTP/1.1 200 OK\r\n" \
                                "Content-Type: text/html\r\n" \
                                "Content-Length: 98\r\n" \
                                "<html>\r\n" \
                                "<head>\r\n" \
                                "<title>OpenSSL demo</title></head><body>\r\n" \
                                "OpenSSL server demo!\r\n" \
                                "</body>\r\n" \
                                "</html>\r\n" \
                                "\r\n"

void CoAP_Init( EventGroupHandle_t wifi_event_group )
{
    int ret;
    xTaskHandle coap_handle;

    assert( wifi_event_group != NULL );
    wifi_events = wifi_event_group;

    ret = xTaskCreate( 
        CoAP_Thread, 
        COAP_THREAD_NAME,
        COAP_THREAD_STACK_SIZE_WORDS,
        NULL,
        COAP_THREAD_PRIORITY,
        &coap_handle
    );

    if( ret != pdPASS ){
        ESP_LOGE( TAG, "Failed to create thread %s", COAP_THREAD_NAME );
    }
}

void CoAP_Thread( void* p )
{
    ESP_LOGI( TAG, "Hello from %s!", COAP_THREAD_NAME );

	
    const size_t iotnode_crt_length = iotnode_crt_end - iotnode_crt;
    const size_t iotnode_key_length = iotnode_key_end - iotnode_key;  

    int successes = 0, failures = 0, ret;
    ESP_LOGI( TAG, "TLS server task starting...\n");

    const char *pers = "tls_server";

    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_context ssl;
    mbedtls_x509_crt srvcert;
    mbedtls_pk_context pkey;
    mbedtls_ssl_config conf;
    mbedtls_net_context server_ctx;
    mbedtls_net_context client_ctx;

    // DTLS Specific stuff
    mbedtls_ssl_cookie_ctx cookie_ctx;
    mbed_timer_context timer_ctx;

    unsigned char client_ip[16];
    size_t cliip_len;

    /*
     * 0. Initialize the RNG and the session data
     */

    mbed_timer_init( &timer_ctx );

    mbedtls_net_init(&server_ctx);
    mbedtls_net_init(&client_ctx);
    mbedtls_ssl_init( &ssl );
    mbedtls_ssl_config_init( &conf );
    mbedtls_ssl_cookie_init( &cookie_ctx );

    mbedtls_x509_crt_init( &srvcert );
    mbedtls_pk_init( &pkey );
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init( &ctr_drbg );

    // Set up debug logging things
    mbedtls_debug_set_threshold( ESP_LOG_WARN );
    mbedtls_ssl_conf_dbg( &conf, mbed_debug, NULL );

    // Set up timer callbacks which are required for DTLS
    mbedtls_ssl_set_timer_cb( &ssl, &timer_ctx, mbed_timer_set_delay, mbed_timer_get_delay);

    /*
     * 1. Initialize certificates
     */
    ESP_LOGI( TAG, "Loading the server certificate ...");

    ret = mbedtls_x509_crt_parse(&srvcert, (uint8_t*)iotnode_crt, iotnode_crt_length);
    if(ret < 0)
    {
        ESP_LOGE( TAG, "mbedtls_x509_crt_parse returned -0x%x", -ret );
        abort();
    }

    ESP_LOGI( TAG, "ok (%d skipped)", ret);

    ESP_LOGI( TAG, "Loading the server private key..." );
    ret = mbedtls_pk_parse_key(&pkey, (uint8_t *)iotnode_key, iotnode_key_length, NULL, 0);
    if(ret != 0)
    {
        ESP_LOGE( TAG, "mbedtls_pk_parse_key returned - 0x%x", -ret );
        abort();
    }

    ESP_LOGI( TAG, "ok");
    
    ESP_LOGI( TAG, "Seeding the random number generator..." );

    if((ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                    (const unsigned char *) pers,
                                    strlen(pers))) != 0)
    {
        ESP_LOGE( TAG, "mbedtls_ctr_drbg_seed returned %d", ret );
        abort();
    }

    ESP_LOGI( TAG, "ok");

    /*
     * 2. Setup stuff
     */
    ESP_LOGI( TAG, "Setting up the SSL/TLS structure..." ); 

    if( ( ret = mbedtls_ssl_config_defaults( &conf,
                                          MBEDTLS_SSL_IS_SERVER,
                                          MBEDTLS_SSL_TRANSPORT_DATAGRAM, // DTLS over UDP
                                          MBEDTLS_SSL_PRESET_DEFAULT ) ) != 0 )
    {
        ESP_LOGE( TAG, "mbedtls_ssl_config_defaults returned %d", ret);
        goto exit;
    }

    mbedtls_ssl_conf_rng( &conf, mbedtls_ctr_drbg_random, &ctr_drbg );


    ESP_LOGI( TAG, "ok");

    mbedtls_ssl_conf_ca_chain( &conf, srvcert.next, NULL );
    if( ( ret = mbedtls_ssl_conf_own_cert( &conf, &srvcert, &pkey ) ) != 0 )
    {
        ESP_LOGE( TAG, "mbedtls_ssl_conf_own_cert returned %d", ret );
        abort();
    }

    ESP_LOGI( TAG, "Setting up mbed cookies! ..." ); 

    if( ( ret = mbedtls_ssl_cookie_setup( &cookie_ctx,
                                          mbedtls_ctr_drbg_random, &ctr_drbg ) ) != 0 )
    {
        ESP_LOGE( TAG, "mbedtls_ssl_cookie_setup returned %d", ret );
        goto exit;
    }

    // TODO Figure out how to do this shit properly >:C
    mbedtls_ssl_conf_dtls_cookies( &conf, NULL, NULL, &cookie_ctx );
    // mbedtls_ssl_conf_dtls_cookies( 
    //     &conf, 
    //     mbedtls_ssl_cookie_write, 
    //     mbedtls_ssl_cookie_check,
    //     &cookie_ctx );
    
    ESP_LOGI( TAG, "Setting up mbed RNG..." ); 

    if( ( ret = mbedtls_ssl_setup( &ssl, &conf ) ) != 0)
    {
        ESP_LOGE( TAG, "mbedtls_ssl_setup returned %d", ret );
        goto exit;
    }

    ESP_LOGI( TAG, "ok" ); 

    while( ( xEventGroupWaitBits( wifi_events, COAP_CONNECTED_BIT, pdFALSE, pdTRUE, 1000 ) & COAP_CONNECTED_BIT ) == pdFAIL );

    ESP_LOGI(TAG, "got connection, binding to host");

    /*
        * 1. Start the connection
        */
    if( ( ret = mbedtls_net_bind( &server_ctx, NULL, LOCAL_PORT, MBEDTLS_NET_PROTO_UDP ) ) != 0)
    {
        ESP_LOGE( TAG, "mbedtls_net_bind returned %d", ret );
        goto exit;
    }

    mbedtls_ssl_session_reset( &ssl );
    while(1) {
        mbedtls_net_free(&client_ctx);

        //ESP_LOGI( TAG, "top of loop, free heap = %u\n", xPortGetFreeHeapSize() );

        //ESP_LOGI( TAG, "ok");

        if( ( ret = mbedtls_net_accept( &server_ctx, &client_ctx, client_ip, sizeof( client_ip ) , &cliip_len ) ) != 0 ){
            ESP_LOGE( TAG, "Failed to accept connection" );
            goto exit;
        }

        ESP_LOGI( TAG, "New connection from %d.%d.%d.%d", client_ip[0], client_ip[1], client_ip[2], client_ip[3] );

        //TODO This stuff (DTLS Cookie handshake thingy)
        mbedtls_ssl_set_client_transport_id( &ssl, client_ip, cliip_len );

        mbedtls_ssl_set_bio( &ssl, &client_ctx, mbedtls_net_send, mbedtls_net_recv, mbedtls_net_recv_timeout );

        /*
         * 4. Handshake
         */
        ESP_LOGI( TAG, "Performing the SSL/TLS handshake..." );

        do ret = mbedtls_ssl_handshake( &ssl );
        while( ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE );
        
        if( ret == MBEDTLS_ERR_SSL_HELLO_VERIFY_REQUIRED )
        {
            ESP_LOGI( TAG, "hello verification requested" );
            ret = 0;
            mbedtls_net_free(&client_ctx);
            goto exit;
        }
        else
        {
            ESP_LOGE( TAG, "mbedtls_ssl_handshake returned -0x%x", -ret );
            goto exit;
        }

        ESP_LOGI( TAG, "ok" );


        /*
         * 3. Write the GET request
         */
        ESP_LOGI( TAG, "Writing status to new client..." );

        struct sockaddr_in peer_addr;
        socklen_t peer_addr_len = sizeof(struct sockaddr_in);
        getpeername(client_ctx.fd, (struct sockaddr *)&peer_addr, &peer_addr_len);
        unsigned char buf[256];
        int len = sprintf((char *) buf, "O hai, client " IPSTR ":%d\nFree heap size is %d bytes\n",
                          IP2STR((struct ip4_addr*)&peer_addr.sin_addr),
                          peer_addr.sin_port, xPortGetFreeHeapSize());
        while((ret = mbedtls_ssl_write(&ssl, buf, len)) <= 0)
        {
            if(ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
            {
                ESP_LOGE( TAG, "mbedtls_ssl_write returned %d", ret );
                goto exit;
            }
        }

        len = ret;
        ret = 0;
        ESP_LOGI( TAG, "%d bytes written. Closing socket on client.\n%s", len, (char *) buf);

        mbedtls_ssl_close_notify(&ssl);

    exit:

        if(ret != 0)
        {
            char error_buf[100];
            mbedtls_strerror(ret, error_buf, 100);
            ESP_LOGE( TAG, "Last error was: %d - %s", ret, error_buf );
            failures++;
        } else {
            successes++;
        }

        ESP_LOGI( TAG, "successes = %d failures = %d", successes, failures );
        ESP_LOGI( TAG, "Waiting for next client..." );
    }

    vTaskDelete( NULL );
    return;
}