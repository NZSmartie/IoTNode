#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "assert.h"

#include "esp_log.h"

#include "interfaces/mbed.h"
#include "interfaces/lobaro-coap.h"

static const char *TAG = "CoAP";
static EventGroupHandle_t wifi_events;

const int COAP_CONNECTED_BIT = ( 1 << 0 );

static const CoapOptions_t *_options;

static void coap_thread( void* p )
{
    ESP_LOGI( TAG, "Hello from %s!", COAP_THREAD_NAME );

    while( ( xEventGroupWaitBits( wifi_events, COAP_CONNECTED_BIT, pdFALSE, pdTRUE, 1000 ) & COAP_CONNECTED_BIT ) == pdFAIL );

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

void coap_init( const CoapOptions_t *options, EventGroupHandle_t wifi_event_group )
{
    int ret;
    xTaskHandle coap_handle;

    assert( options != NULL );
    _options = options;

    assert( wifi_event_group != NULL );
    wifi_events = wifi_event_group;

    ret = xTaskCreate( 
        coap_thread,
        COAP_THREAD_NAME,
        COAP_THREAD_STACK_SIZE_WORDS,
        NULL,
        COAP_THREAD_PRIORITY,
        &coap_handle
    );

    if( ret != pdPASS ){
        ESP_LOGE( TAG, "Failed to create thread %s", COAP_THREAD_NAME );
        abort();
    }
}