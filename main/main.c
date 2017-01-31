#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "coap.h"

static bool connected = false;
static EventGroupHandle_t wifi_event_group;

#define STRING2(x) #x
#define STRING(x) STRING2(x)

#if !defined( WIFI_SSID ) || !defined( WIFI_PASSWORD )
    #error WIFI_SSID or WIFI_PASSWORD not set in secrets file. See secrets.example
#endif

extern const unsigned char iotnode_crt[]     asm("_binary_iotnode_crt_start");
extern const unsigned char iotnode_crt_end[] asm("_binary_iotnode_crt_end");

extern const unsigned char iotnode_key[]     asm("_binary_iotnode_key_start");
extern const unsigned char iotnode_key_end[] asm("_binary_iotnode_key_end");

CoapOptions_t coap_options = ( CoapOptions_t ){
    .flags = {
        .useDTLS = 1
    },
    .DTLS = {
        .cert_ptr = iotnode_crt,
        .cert_key_ptr = iotnode_key,
    }
};

esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch ( event->event_id )
    {
        case SYSTEM_EVENT_STA_CONNECTED:
            connected = true;
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            xEventGroupSetBits( wifi_event_group, COAP_CONNECTED_BIT );
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            xEventGroupClearBits( wifi_event_group, COAP_CONNECTED_BIT );
            connected = false;
            break;
        default:
            break;
    }
    return ESP_OK;
}

void app_main(void)
{
    coap_options.DTLS.cert_len = iotnode_crt_end - iotnode_crt;
    coap_options.DTLS.cert_key_len = iotnode_key_end - iotnode_key;  

    wifi_event_group = xEventGroupCreate();

    nvs_flash_init();
    tcpip_adapter_init();
    
    coap_init( &coap_options, wifi_event_group );

    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    wifi_config_t sta_config = {
        .sta = {
            .ssid = STRING( WIFI_SSID ),
            .password = STRING( WIFI_PASSWORD ),
            .bssid_set = false
        }
    };
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &sta_config) );

    ESP_ERROR_CHECK( esp_wifi_start() );
    ESP_ERROR_CHECK( esp_wifi_connect() );

    gpio_set_direction(GPIO_NUM_4, GPIO_MODE_OUTPUT);
    int level = 0;
    while (true) {
        gpio_set_level(GPIO_NUM_4, level);
        level = !level;

        if ( connected )
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        else
            vTaskDelay(250 / portTICK_PERIOD_MS);
    }
}

