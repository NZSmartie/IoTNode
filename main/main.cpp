#include <cstring>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "sdkconfig.h"

#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "esp_log.h"

#include "interfaces/lobarocoap.h"
#include "resources/wifi.h"

static const char* TAG = "IoTNode";
static bool connected = false;
static EventGroupHandle_t wifi_event_group;

#define STRING2(x) #x
#define STRING(x) STRING2(x)

#if !defined( WIFI_SSID ) || !defined( WIFI_PASSWORD )
    #error WIFI_SSID or WIFI_PASSWORD not set in secrets file. See secrets.example
#endif

esp_err_t event_handler(void *ctx, system_event_t *event)
{
    int ret;
    switch ( event->event_id )
    {
        case SYSTEM_EVENT_STA_START:
            // Change the default hostname (can only be done when interface has started)
            if ( (ret = tcpip_adapter_set_hostname( TCPIP_ADAPTER_IF_STA, CONFIG_IOTNODE_HOSTNAME ) ) != ESP_OK )
                ESP_LOGE( TAG, "tcpip_adapter_set_hostname failed to set Hostname to \"" CONFIG_IOTNODE_HOSTNAME "\" with %d (0x%X)", ret, ret );
            break;
        case SYSTEM_EVENT_STA_CONNECTED:
            connected = true;
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            xEventGroupSetBits( wifi_event_group, kCoapConnectedBit );
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            xEventGroupClearBits( wifi_event_group, kCoapConnectedBit );
            connected = false;
            break;
        default:
            break;
    }
    return ESP_OK;
}

extern "C"
void app_main(void)
{
    wifi_event_group = xEventGroupCreate();

    nvs_flash_init();
    tcpip_adapter_init();

    CoapResult result;
    LobaroCoap coap_interface(result);
    assert(result == CoapResult::OK);

    // Create and register our wifi resource
    WifiResource wifiResource(coap_interface);

    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );

    wifi_config_t sta_config = {};
    std::strcpy((char*)sta_config.sta.ssid, STRING(WIFI_SSID));
    std::strcpy((char*)sta_config.sta.password, STRING(WIFI_PASSWORD));
    sta_config.sta.bssid_set = false;

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
