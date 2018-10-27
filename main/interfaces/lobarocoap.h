#ifndef _INTERFACES_LOBAROCOAP_H_
#define _INTERFACES_LOBAROCOAP_H_

#include <vector>
#include "../coap.h"

extern "C" {
    #include "liblobaro_coap.h"
}

static const int kCoapMemorySize = 4096;

class LobaroCoap : public ICoapInterface
{
private:
    xTaskHandle _task;
    static void TaskHandle(void* pvParameters);
    CoAP_Socket_t *_context;
    struct netconn *_socket;
    bool _networkReady;
    static bool SendDatagram(SocketHandle_t socketHandle, NetPacket_t* packet);

    bool SendDatagram(NetPacket_t* packet);
    void ReadDatagram();
public:
    LobaroCoap();
    void Start(CoapResult &result);
    virtual ~LobaroCoap(){}

    void CreateResource(CoapResource &resource, IApplicationResource * const applicationResource, const char* uri, CoapResult &result);
    void SetNetworkReady(bool ready);
};

class LobaroCoapResource : public ICoapResource
{
    static std::vector<LobaroCoapResource*> _resources;
    CoAP_Res_t *_resource;
    static CoAP_HandlerResult_t ResourceHandler(CoAP_Message_t *request, CoAP_Message_t *response);
public:
    LobaroCoapResource(IApplicationResource * const applicationResource, const char* uri, CoapResult &result);

    virtual ~LobaroCoapResource()
    {
        for(auto it = _resources.begin(); it != _resources.end(); it++)
        {
            if (*it == this)
                _resources.erase(it);
        }
    }

    void RegisterHandler(CoapMessageCode requestType, CoapResult &result);
};

class LobaroCoapMessage : public ICoapMessage
{
    CoAP_Message_t * const _message;
public:
    ~LobaroCoapMessage(){}
    LobaroCoapMessage(CoAP_Message_t *message)
        : _message(message) {}

    void GetOption(CoapOption &option,const uint16_t number, CoapResult &result) const;
    void AddOption(ICoapOption const *option, CoapResult &result);

    void GetCode(CoapMessageCode &code, CoapResult &result) const;
    void SetCode(CoapMessageCode code, CoapResult &result);

    void GetPayload(Payload &payload, CoapResult &result) const;
    void SetPayload(const Payload &payload, CoapResult &result);
};

#endif // _INTERFACES_LOBAROCOAP_H_
