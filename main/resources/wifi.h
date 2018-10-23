#ifndef _RESOURCES_WIFI_H_
#define _RESOURCES_WIFI_H_

#include "../coap.h"

class WifiResource : public IApplicationResource {
    ICoapInterface& _coap;
    CoapResource _resource;
public:
    WifiResource(ICoapInterface& coap);
    void HandleRequest(ICoapMessage const *request, ICoapMessage *response, CoapResult &result);
};

#endif /* _RESOURCES_WIFI_H_ */
