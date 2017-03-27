#ifndef _INTERFACES_LOBARO_COAP_H_
#define _INTERFACES_LOBARO_COAP_H_

#include "../coap.h"

int lobaro_coap_init( void );
void lobaro_coap_do_work( void );

CoapInterface_t CoapGetInterface( void );

#endif /* _INTERFACES_LOBARO_COAP_H_ */