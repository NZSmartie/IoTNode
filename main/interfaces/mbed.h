
#ifndef _INTERFACES_MBED_H
#define _INTERFACES_MBED_H 

enum mbed_timer_state
{
    MBED_TIMER_STATE_CANCELLED = -1,
    MBED_TIMER_STATE_RUNNING = 0,
    MBED_TIMER_STATE_PASSED_INT = 1,
    MBED_TIMER_STATE_PASSED_FIN = 2
};

typedef struct{
    void *d_ctx;
    volatile uint32_t int_ms;
    volatile uint32_t fin_ms;
	volatile int8_t state;
} mbed_timer_context;

void mbed_timer_init( mbed_timer_context *ctx );

void mbed_timer_set_delay(void *data, uint32_t int_ms, uint32_t fin_ms );

int mbed_timer_get_delay(void *data);

void mbed_debug( void *ctx, int level, const char *file, int line, const char *str );

#endif /* _INTERFACES_MBED_H */