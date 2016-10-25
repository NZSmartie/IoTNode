
#ifndef _PERIPHERAL_TIMER_H
#define _PERIPHERAL_TIMER_H

typedef struct{
    unsigned char opaque[4];
} timer_delay_context;

void timer_init(void);

void timer_delay_init(timer_delay_context *delay_ctx);

int timer_delay_get_lapsed(timer_delay_context *delay_ctx, bool reset);

void timer_delay_ms(unsigned int ms);

#endif /* _PERIPHERAL_TIMER_H */