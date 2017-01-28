#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include "sdkconfig.h"
#include "esp_log.h"

#include "mbed.h"

void mbed_timer_init( mbed_timer_context *ctx )
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

void mbed_timer_set_delay( void *data, uint32_t int_ms, uint32_t fin_ms ) {
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

int mbed_timer_get_delay( void *data ) {
	// Assume our my_timer_context type is used for data
	mbed_timer_context *ctx = data;

	return ctx->state;
}

void mbed_debug( void *ctx, int level, const char *file, int line, const char *str ) 
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