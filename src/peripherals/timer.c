#include <stdbool.h>
#include <stdint.h>

#include "espressif/esp_common.h"
#include "esp8266.h"
#include "esp/timer.h"

#include "../config.h"
#include "timer.h"

#define PASTER(x,y) x ## y
#define CONCAT(x,y) PASTER(x,y)
#define INUM_TIMER_SOUECE(x) CONCAT(INUM_TIMER_, x)

typedef struct{
    uint32_t runner;
    bool running;   
}_timer_context;

typedef struct{
    uint32_t offset;
} _timer_delay_context;

// Current Timer Context
static _timer_context _c_ctx = {0};

static inline bool _is_running(){
    return _c_ctx.running;
}

static void timer_interrupt_handler(void){
    if(!_c_ctx.running)
        return;
    
    _c_ctx.runner++;
}

void timer_init(){
    _c_ctx.runner = 0;

    timer_set_interrupts(TIMER_SOURCE, false);
    timer_set_run(TIMER_SOURCE, false);
    // set up ISRs
    _xt_isr_attach(INUM_TIMER_SOUECE(TIMER_SOURCE), timer_interrupt_handler);
    timer_set_frequency(TIMER_SOURCE, 1000);

    timer_set_interrupts(TIMER_SOURCE, true);
    timer_set_run(TIMER_SOURCE, true);

    _c_ctx.running = true;
}

void timer_delay_init(timer_delay_context *delay_ctx){
    _timer_delay_context *ctx = (_timer_delay_context*)delay_ctx;
    _c_ctx.runner = ctx->offset;
}

int timer_delay_get_lapsed(timer_delay_context *delay_ctx, bool reset){
    uint32_t ms;
    _timer_delay_context *ctx = (_timer_delay_context*)delay_ctx;
    
    if(!_is_running())
        timer_init();

    ms = _c_ctx.runner - ctx->offset;
    if(reset)
        _c_ctx.runner = ctx->offset;
    return ms;
}

void timer_delay_ms(unsigned int ms){
    timer_delay_context delay_ctx;

    if(!_is_running())
        timer_init();
        
    timer_delay_init(&delay_ctx);

    while(timer_delay_get_lapsed(&delay_ctx, false) < ms) { };
}