#ifndef _OIC_H_
#define _OIC_H_

#include "coap.h"

extern const char *OIC_DEVICE_UUID;

void oic_init( const CoapInterface_t interface );

void oic_register_resource( const char *base_uri );

#endif /* _OIC_H_ */