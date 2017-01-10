#include "lobaro-coap/interface/coap_interface.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

static const char* TAG = "Lobaro CoAP";

//Uart/Display function to print debug/status messages to
void hal_uart_puts( char *s ) {
    ESP_LOGD( TAG, "%s", s );
}

void hal_uart_putc( char c ){
    ESP_LOGD( TAG, "%c", c );
}

static TimerHandle_t timer_handle = NULL;
uint64_t seconds = 0;

static void timer_handle_callback( TimerHandle_t xTimer ){
    seconds++;
}

//1Hz Clock used by timeout logic
uint32_t hal_rtc_1Hz_Cnt( void ){
    if( timer_handle == NULL ) {
        timer_handle = xTimerCreate( "Lobaro CoAP", configTICK_RATE_HZ, pdTRUE, (void *) 0, timer_handle_callback );
        if( timer_handle != NULL )
            xTimerStart( timer_handle, 0 );
    }

    return seconds;
}

//Non volatile memory e.g. flash/sd-card/eeprom
//used to store observers during deepsleep of server
uint8_t* hal_nonVolatile_GetBufPtr(){
    return NULL;
}

bool hal_nonVolatile_WriteBuf( uint8_t* data, uint32_t len ){
    return false;
}