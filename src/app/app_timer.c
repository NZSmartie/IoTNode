#include <stdbool.h>
#include <stdint.h>

#include "app_timer.h"
#include "../peripherals/timer.h"

void app_timer_set_delay(void *data, uint32_t int_ms, uint32_t fin_ms ){
    // Assume our my_timer_context type is used for data
    app_timer_context *ctx = data;

    ctx->int_ms = int_ms;
    ctx->fin_ms = fin_ms;
    timer_delay_init(&ctx->d_ctx);
}

int app_timer_get_delay(void *data){
    // Assume our my_timer_context type is used for data
    app_timer_context *ctx = data;
    uint32_t lapsed;

    if(ctx->fin_ms==0)
        return -1;
    
    lapsed = timer_delay_get_lapsed(&ctx->d_ctx, false);

    if(lapsed >= ctx->fin_ms)
        return 2;

    if(lapsed >= ctx->int_ms)
        return 1;

    return 0;
}