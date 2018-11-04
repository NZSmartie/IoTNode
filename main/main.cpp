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
#include "resources/led.h"
#include "resources/switch.h"
#include "resources/wifi.h"

static const char* kTag = "IoTNode";
static bool connected = false;
static EventGroupHandle_t wifi_event_group;

static const int kCoapConnectedBit = ( 1 << 0 );
static const gpio_num_t kLEDRedPin = GPIO_NUM_26;
static const gpio_num_t kLEDGreenPin = GPIO_NUM_33;
static const gpio_num_t kLEDBluePin = GPIO_NUM_32;

static const gpio_num_t kSwitchPin = GPIO_NUM_12;

static const char *kThreadName = "Main Task";
static const size_t kThreadStackSize = 4098;
static const UBaseType_t kThreadPriority = 8;

#define STRING2(x) #x
#define STRING(x) STRING2(x)

#if !defined( WIFI_SSID ) || !defined( WIFI_PASSWORD )
    #error WIFI_SSID or WIFI_PASSWORD not set in secrets file. See secrets.example
#endif

LobaroCoap coap_interface;

esp_err_t event_handler(void *ctx, system_event_t *event)
{
    int ret;
    switch ( event->event_id )
    {
        case SYSTEM_EVENT_STA_START:
            // Change the default hostname (can only be done when interface has started)
            if ( (ret = tcpip_adapter_set_hostname( TCPIP_ADAPTER_IF_STA, CONFIG_IOTNODE_HOSTNAME ) ) != ESP_OK )
                ESP_LOGE( kTag, "tcpip_adapter_set_hostname failed to set Hostname to \"" CONFIG_IOTNODE_HOSTNAME "\" with %d (0x%X)", ret, ret );
            break;
        case SYSTEM_EVENT_STA_CONNECTED:
            connected = true;
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            xEventGroupSetBits( wifi_event_group, kCoapConnectedBit );
            coap_interface.SetNetworkReady(true);
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            xEventGroupClearBits( wifi_event_group, kCoapConnectedBit );
            connected = false;
            coap_interface.SetNetworkReady(false);
            esp_wifi_connect();
            break;
        default:
            break;
    }
    return ESP_OK;
}

static void TaskHandle(void* pvParameters);

extern "C"
void app_main(void)
{
    wifi_event_group = xEventGroupCreate();

    nvs_flash_init();
    tcpip_adapter_init();

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

    // Start a new Application task with enough  stack size to hold the resources
    xTaskHandle _task;
    int ret = xTaskCreate(
        &TaskHandle,
        kThreadName,
        kThreadStackSize,
        nullptr,
        kThreadPriority,
        &_task
    );

    if (ret != true)
        ESP_LOGE(kTag, "Failed to create thread %s", kThreadName );

    while (true)
    {
        vTaskSuspend(nullptr);
    }
}

static void TaskHandle(void* pvParameters)
{
    CoapResult result;
    coap_interface.Start(result);
    assert(result == CoapResult::OK);

    // Create and register our wifi resource
    WifiResource wifiResource(coap_interface);

    // Create and register our LED resource
    LEDResource statusLED(coap_interface, kLEDRedPin, kLEDGreenPin, kLEDBluePin);
    SwitchResource pushSwitch(coap_interface, kSwitchPin);

    int level = 0;
    bool lastConnectedState = false;
    while (true)
    {

        if (connected)
        {
            if(lastConnectedState != connected)
            {
                lastConnectedState = connected;
                statusLED.SetStatusColor(0, 15, 0, 250);
            }
            else
            {
                statusLED.SetMode(LEDResource::Mode::User);
            }

            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
        else
        {
            level = level ? 0 : 1;

            statusLED.SetMode(LEDResource::Mode::ShowStatus); // force the led to show the status instead of use configured colour
            statusLED.SetStatusColor(level * 30, 0, 0, 100);
            vTaskDelay(250 / portTICK_PERIOD_MS);
        }
    }
}
