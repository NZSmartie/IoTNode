#ifndef _INTERFACES_LOBARO_COAP_H_
#define _INTERFACES_LOBARO_COAP_H_

#include "../coap.h"

void lobaro_coap_init( void );
int lobaro_coap_listen( void );
void lobaro_coap_do_work( void );

CoapInterface_t CoapGetInterface( void );

#endif /* _INTERFACES_LOBARO_COAP_H_ */