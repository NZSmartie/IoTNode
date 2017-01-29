#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "string.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "lwip/api.h"

#include "tcpip_adapter.h"

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include "sdkconfig.h"
#include "esp_log.h"

#include "mbed.h"

#include "tcpip_adapter.h"

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

static const char *TAG = "mbed";
static const char *LOCAL_PORT = "4433";

enum mbed_timer_state
{
    MBED_TIMER_STATE_CANCELLED = -1,
    MBED_TIMER_STATE_RUNNING = 0,
    MBED_TIMER_STATE_PASSED_INT = 1,
    MBED_TIMER_STATE_PASSED_FIN = 2
};

typedef struct
{
    void *d_ctx;
    volatile uint32_t int_ms;
    volatile uint32_t fin_ms;
	volatile int8_t state;
} mbed_timer_context;

static int successes = 0, failures = 0, ret;
static const char *pers = "tls_server";
static const CoapDtlsOptions_t *_options;

static mbedtls_entropy_context entropy;
static mbedtls_ctr_drbg_context ctr_drbg;
static mbedtls_ssl_context ssl;
static mbedtls_x509_crt srvcert;
static mbedtls_pk_context pkey;
static mbedtls_ssl_config conf;
static mbedtls_net_context server_ctx;
static mbedtls_net_context client_ctx;

// DTLS Specific stuff
static mbedtls_ssl_cookie_ctx cookie_ctx;
static mbed_timer_context timer_ctx;

static EventGroupHandle_t wifi_events;

static void mbed_timer_init( mbed_timer_context *ctx )
{
	ctx->d_ctx = NULL;
	ctx->int_ms = 0;
    ctx->fin_ms = 0;
	ctx->state = MBED_TIMER_STATE_CANCELLED;
}

static void mbed_timer_callback(TimerHandle_t xTimer) 
{
	mbed_timer_context *ctx = pvTimerGetTimerID( xTimer );
	if ( ctx->state == MBED_TIMER_STATE_RUNNING )
	{
		ctx->state = MBED_TIMER_STATE_PASSED_INT;
		xTimerChangePeriod( xTimer, pdMS_TO_TICKS( ctx->fin_ms ), 0 );
		xTimerStart( xTimer, 0 );
	}
	else if( ctx->state == MBED_TIMER_STATE_PASSED_INT )
	{
		ctx->state = MBED_TIMER_STATE_PASSED_FIN;
		xTimerStop( xTimer, 0 );
	}
}

static void mbed_timer_set_delay( void *data, uint32_t int_ms, uint32_t fin_ms ) 
{
	// Assume our my_timer_context type is used for data
	mbed_timer_context *ctx = data;

	TickType_t ticks;

	ctx->int_ms = int_ms;
	ctx->fin_ms = fin_ms;

	// if final interval is 0, then cancel the timer
	if( fin_ms == 0 )
	{
		ctx->state = MBED_TIMER_STATE_CANCELLED;
		if( ctx->d_ctx != NULL && xTimerIsTimerActive( ctx->d_ctx ) == pdTRUE )
		  xTimerStop( ctx->d_ctx, 0 );
		return;
	}
	else if ( int_ms != 0 ) 
	{
		ctx->state = MBED_TIMER_STATE_RUNNING;
		ticks = pdMS_TO_TICKS(int_ms);
	}
	else
	{
		ctx->state = MBED_TIMER_STATE_PASSED_INT;
		ticks = pdMS_TO_TICKS( fin_ms );
	}

	if( ctx->d_ctx == NULL ){
		ctx->d_ctx = xTimerCreate( "mbed Timer", ticks, pdFALSE, ctx, mbed_timer_callback );
		xTimerStart( ctx->d_ctx, 0 );
	} else {
		xTimerChangePeriod( ctx->d_ctx, ticks, 0 );
		if( xTimerIsTimerActive( ctx->d_ctx ) == pdFALSE )
			xTimerStart(ctx->d_ctx, 0);
	}
}

static int mbed_timer_get_delay( void *data )
{
	// Assume our my_timer_context type is used for data
	mbed_timer_context *ctx = data;

	return ctx->state;
}

static void mbed_debug( void *ctx, int level, const char *file, int line, const char *str ) 
{
	const char *tag = "mbed";
	if (LOG_LOCAL_LEVEL < level)
		return;

	switch (level)
	{
	case ESP_LOG_ERROR:
		ESP_LOGE( tag, "%s", str );
		break;
	case ESP_LOG_WARN:
		ESP_LOGW( tag, "%s", str );
		break;
	case ESP_LOG_INFO:
		ESP_LOGI( tag, "%s", str );
		break;
	case ESP_LOG_DEBUG:
		ESP_LOGD( tag, "%s", str );
		break;
	case ESP_LOG_VERBOSE:
		ESP_LOGV( tag, "%s", str );
		break;
	default:
		break;
	}
}

int mbed_init_dtls( const CoapDtlsOptions_t *options, EventGroupHandle_t wifi_event_group )
{
    _options = options;
	wifi_events = wifi_event_group;

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
    mbedtls_debug_set_threshold( ESP_LOG_ERROR );
    mbedtls_ssl_conf_dbg( &conf, mbed_debug, NULL );

    // Set up timer callbacks which are required for DTLS
    mbedtls_ssl_set_timer_cb( &ssl, &timer_ctx, mbed_timer_set_delay, mbed_timer_get_delay);

    /*
     * 1. Initialize certificates
     */
    ESP_LOGD( TAG, "Loading the server certificate ...");

    ret = mbedtls_x509_crt_parse( &srvcert, _options->cert_ptr, _options->cert_len );
    if(ret < 0)
    {
        ESP_LOGE( TAG, "mbedtls_x509_crt_parse returned -0x%x", -ret );
        return ret;
    }

    ESP_LOGD( TAG, "ok (%d skipped)", ret);

    ESP_LOGD( TAG, "Loading the server private key..." );
    ret = mbedtls_pk_parse_key( &pkey, _options->cert_key_ptr, _options->cert_key_len, NULL, 0 );
    if(ret != 0)
    {
        ESP_LOGE( TAG, "mbedtls_pk_parse_key returned - 0x%x", -ret );
        return ret;
    }

    ESP_LOGD( TAG, "ok");
    
    ESP_LOGD( TAG, "Seeding the random number generator..." );

    if((ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                    (const unsigned char *) pers,
                                    strlen(pers))) != 0)
    {
        ESP_LOGE( TAG, "mbedtls_ctr_drbg_seed returned %d", ret );
        return ret;
    }

    ESP_LOGD( TAG, "ok");

    /*
     * 2. Setup stuff
     */
    ESP_LOGD( TAG, "Setting up the SSL/TLS structure..." ); 

    if( ( ret = mbedtls_ssl_config_defaults( &conf,
                                          MBEDTLS_SSL_IS_SERVER,
                                          MBEDTLS_SSL_TRANSPORT_DATAGRAM, // DTLS over UDP
                                          MBEDTLS_SSL_PRESET_DEFAULT ) ) != 0 )
    {
        ESP_LOGE( TAG, "mbedtls_ssl_config_defaults returned %d", ret);
        return ret;
    }

    mbedtls_ssl_conf_rng( &conf, mbedtls_ctr_drbg_random, &ctr_drbg );


    ESP_LOGD( TAG, "ok");

    mbedtls_ssl_conf_ca_chain( &conf, srvcert.next, NULL );
    if( ( ret = mbedtls_ssl_conf_own_cert( &conf, &srvcert, &pkey ) ) != 0 )
    {
        ESP_LOGE( TAG, "mbedtls_ssl_conf_own_cert returned %d", ret );
        return ret;
    }

    ESP_LOGD( TAG, "Setting up mbed cookies! ..." ); 

    if( ( ret = mbedtls_ssl_cookie_setup( &cookie_ctx,
                                          mbedtls_ctr_drbg_random, &ctr_drbg ) ) != 0 )
    {
        ESP_LOGE( TAG, "mbedtls_ssl_cookie_setup returned %d", ret );
        return ret;
    }

    // TODO Figure out how to do this shit properly >:C
    mbedtls_ssl_conf_dtls_cookies( &conf, NULL, NULL, &cookie_ctx );
    // mbedtls_ssl_conf_dtls_cookies( 
    //     &conf, 
    //     mbedtls_ssl_cookie_write, 
    //     mbedtls_ssl_cookie_check,
    //     &cookie_ctx );
    
    ESP_LOGD( TAG, "Setting up mbed RNG..." ); 

    if( ( ret = mbedtls_ssl_setup( &ssl, &conf ) ) != 0)
    {
        ESP_LOGE( TAG, "mbedtls_ssl_setup returned %d", ret );
        return ret;
    }

    ESP_LOGD( TAG, "ok" ); 

    while( ( xEventGroupWaitBits( wifi_events, COAP_CONNECTED_BIT, pdFALSE, pdTRUE, 1000 ) & COAP_CONNECTED_BIT ) == pdFAIL );

    ESP_LOGD(TAG, "Connected event bit set: Binding to host");

    /*
        * 1. Start the connection
        */
    if( ( ret = mbedtls_net_bind( &server_ctx, NULL, LOCAL_PORT, MBEDTLS_NET_PROTO_UDP ) ) != 0)
    {
        ESP_LOGE( TAG, "mbedtls_net_bind returned %d", ret );
        return ret;
    }

    ESP_LOGI(TAG, "DTLS server is listening on 0.0.0.0:%s", LOCAL_PORT );

	return 0;
}

int mbed_do_task( void )
{
	unsigned char client_ip[16];
    size_t cliip_len;

	mbedtls_ssl_session_reset( &ssl );
	mbedtls_net_free(&client_ctx);

	ESP_LOGD( TAG, "top of loop, free heap = %u\n", xPortGetFreeHeapSize() );

	if( ( ret = mbedtls_net_accept( &server_ctx, &client_ctx, client_ip, sizeof( client_ip ) , &cliip_len ) ) != 0 ){
		ESP_LOGE( TAG, "Failed to accept connection" );
		return ret;
	}

	{
		struct sockaddr_in peer_addr;
		socklen_t peer_addr_len = sizeof( struct sockaddr_in );
		getpeername( client_ctx.fd, ( struct sockaddr * ) &peer_addr, &peer_addr_len );

		ESP_LOGI( TAG, "New client connection from " IPSTR ":%d", IP2STR((struct ip4_addr*)&peer_addr.sin_addr), peer_addr.sin_port );
	}

	//TODO This stuff (DTLS Cookie handshake thingy)
	mbedtls_ssl_set_client_transport_id( &ssl, client_ip, cliip_len );

	mbedtls_ssl_set_bio( &ssl, &client_ctx, mbedtls_net_send, mbedtls_net_recv, mbedtls_net_recv_timeout );

	/*
		* 4. Handshake
		*/
	ESP_LOGD( TAG, "Performing the SSL/TLS handshake..." );

	do ret = mbedtls_ssl_handshake( &ssl );
	while( ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE );
	
	if( ret == MBEDTLS_ERR_SSL_HELLO_VERIFY_REQUIRED )
	{
		ESP_LOGI( TAG, "Hello verification requested, resetting TLS session" );
		ret = 0;
		mbedtls_net_free(&client_ctx);
		goto exit;
	}
	else if( ret != 0 )
	{
		ESP_LOGE( TAG, "mbedtls_ssl_handshake returned -0x%x", -ret );
		goto exit;
	}

	ESP_LOGD( TAG, "ok" );


	/*
		* 3. Write the GET request
		*/
	ESP_LOGD( TAG, "Writing status to new client..." );

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
	ESP_LOGD( TAG, "%d bytes written", len );
	ESP_LOGI( TAG, "Closing socket on client" );

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

	ESP_LOGI( TAG, "Waiting for next client..." );
	return 0;
}