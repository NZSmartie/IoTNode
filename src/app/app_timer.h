
#ifndef _APP_APP_TIMER_H
#define _APP_APP_TIMER_H 

#include "../peripherals/timer.h"

typedef struct{
    timer_delay_context d_ctx;
    volatile uint32_t int_ms;
    volatile uint32_t fin_ms;
} app_timer_context;

void app_timer_set_delay(void *data, uint32_t int_ms, uint32_t fin_ms );

int app_timer_get_delay(void *data);

#endif /* _APP_APP_TIMER_H */