#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "assert.h"

#include "esp_log.h"

#include "interfaces/mbed.h"
#include "interfaces/lobaro-coap.h"

static const char *TAG = "CoAP";
static EventGroupHandle_t wifi_events;

const int kCoapConnectedBit = ( 1 << 0 );

static const CoapOptions_t *_options;

static void coap_thread( void* p )
{
    ESP_LOGI( TAG, "Hello from %s!", COAP_THREAD_NAME );

    while( ( xEventGroupWaitBits( wifi_events, kCoapConnectedBit, pdFALSE, pdTRUE, 1000 ) & kCoapConnectedBit ) == pdFAIL );

    // int ret;
    // if( ( ret = mbed_init_dtls( &_options->DTLS, wifi_events ) ) != 0)
    // {
    //     ESP_LOGE( TAG, "Failed to initialise DTLS server" );
    //     abort();
    // }

    lobaro_coap_init();

    while( 1 )
    {
        lobaro_coap_do_work();
    }

    ESP_LOGE( TAG, "Destroying thread. Bye!" );
    vTaskDelete( NULL );
    return;
}

CoapResult_t coap_init( const CoapOptions_t *options, EventGroupHandle_t wifi_event_group )
{
    int ret;
    xTaskHandle coap_handle;

    if( options == NULL ) return kCoapError;
    _options = options;

    if( wifi_event_group == NULL ) return kCoapError;
    wifi_events = wifi_event_group;

    ret = xTaskCreate(
        coap_thread,
        COAP_THREAD_NAME,
        COAP_THREAD_STACK_SIZE_WORDS,
        NULL,
        COAP_THREAD_PRIORITY,
        &coap_handle
    );

    if( ret != pdPASS )
    {
        ESP_LOGE( TAG, "Failed to create thread %s", COAP_THREAD_NAME );
        return kCoapError;
    }

    return kCoapOK;
}