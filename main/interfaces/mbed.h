
#ifndef _INTERFACES_MBED_H
#define _INTERFACES_MBED_H 

#include "../coap.h"

int mbed_init_dtls( const CoapDtlsOptions_t *options, EventGroupHandle_t wifi_event_group );

int mbed_do_task( void );

#endif /* _INTERFACES_MBED_H */