#ifndef _OIC_H_
#define _OIC_H_

#include "coap.h"

extern const char *OIC_DEVICE_UUID;

typedef enum
{
    kOICInterfaceBaseline   = 0x01,
    kOICInterfaceLinkLists  = 0x02,
    kOICInterfaceBatch      = 0x04,
    kOICInterfaceReadWrite  = 0x08,
    kOICInterfaceReadOnly   = 0x10,
    kOICInterfaceActuator   = 0x20,
    kOICInterfaceSensor     = 0x40,
} OICInterface_t;

typedef struct OICResource_s
{
    char* href;
    OICInterface_t interfaces;
    CoapResourceCallback_t callback;
    size_t resource_types_count;
    char *resource_types[];

} OICResource_t;

void oic_init( const CoapInterface_t interface );

void oic_register_resource( const OICResource_t *resourcce );

#endif /* _OIC_H_ */