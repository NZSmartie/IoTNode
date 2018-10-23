#include <iterator>
#include <string>
#include <new>
#include "assert.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include "lwip/api.h"

extern "C" {
    #include "liblobaro_coap.h"
    #include "coap_options.h"
    #include "option-types/coap_option_cf.h"
    #include "interface/network/net_Endpoint.h"
}

#include "lobarocoap.h"

#ifdef LOG_LOCAL_LEVEL
    #undef LOG_LOCAL_LEVEL
#endif

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include "esp_log.h"

// Custom Resources

static const char* kTag = "Lobaro-CoAP";
static const char* kCoapThreadName = "libcoap";

static const uint16_t kCoapPort = 5683;
static const uint16_t kCoapPortDtls = 5684;
static const int kCoapDefaultTimeUSec = 0;
static const int kCoapDefaultTimeSec = 5;
static const int kCoapThreadStackSize = 10240;
static const int kCoapThreadPriority = 8;

const int kCoapConnectedBit = ( 1 << 0 );

static uint8_t _coap_memory[kCoapMemorySize];
static CoAP_Config_t _coap_config = {_coap_memory, kCoapMemorySize};

static uint32_t hal_rtc_1Hz_Cnt( void );
static void hal_uart_puts( char *s );
CoAP_API_t _coap_api = {&hal_rtc_1Hz_Cnt, &hal_uart_puts};

std::tuple<CoapOptionValue, CoapOptionType> const CoapOptiontypeMap[] =
{
    std::make_tuple(CoapOptionValue::IfMatch, CoapOptionType::Empty),
    std::make_tuple(CoapOptionValue::UriHost, CoapOptionType::Empty),
    std::make_tuple(CoapOptionValue::ETag, CoapOptionType::Empty),
    std::make_tuple(CoapOptionValue::IfNoneMatch, CoapOptionType::Empty),
    std::make_tuple(CoapOptionValue::UriPort, CoapOptionType::Empty),
    std::make_tuple(CoapOptionValue::LocationPath, CoapOptionType::Empty),
    std::make_tuple(CoapOptionValue::UriPath, CoapOptionType::Empty),
    std::make_tuple(CoapOptionValue::ContentFormat, CoapOptionType::Empty),
    std::make_tuple(CoapOptionValue::MaxAge, CoapOptionType::Empty),
    std::make_tuple(CoapOptionValue::UriQuery, CoapOptionType::Empty),
    std::make_tuple(CoapOptionValue::Accept, CoapOptionType::Empty),
    std::make_tuple(CoapOptionValue::LocationQuery, CoapOptionType::Empty),
    std::make_tuple(CoapOptionValue::ProxyUri, CoapOptionType::Empty),
    std::make_tuple(CoapOptionValue::ProxyScheme, CoapOptionType::Empty),
    std::make_tuple(CoapOptionValue::Size1, CoapOptionType::Empty),
};

std::vector<LobaroCoapResource*> LobaroCoapResource::_resources;

LobaroCoap::LobaroCoap(CoapResult &result)
{
    CoAP_Init(_coap_api, _coap_config);

    int ret = xTaskCreate(
        &LobaroCoap::TaskHandle,
        kCoapThreadName,
        kCoapThreadStackSize,
        this,
        kCoapThreadPriority,
        &this->_task
    );

    result = ret ? CoapResult::OK : CoapResult::Error;

    if (ret != true)
        ESP_LOGE( kTag, "Failed to create thread %s", kCoapThreadName );
}

bool LobaroCoap::SendDatagram(NetPacket_t *packet)
{
    auto success = false;
    ip_addr_t client_address = IPADDR4_INIT(0);

    if (NETCONNTYPE_GROUP(this->_socket->type) != NETCONN_UDP)
    {
        ESP_LOGE( kTag, "Socket handle is not a datagram type" );
        return false;
    }

    struct netbuf *buffer = netbuf_new();

    for(;;)
    {
        if( netbuf_ref( buffer, packet->pData, packet->size ) != ERR_OK)
        {
            ESP_LOGE( kTag, "netbuf_ref( ... ): failed");
            break;
        }

        if (packet->remoteEp.NetType != IPV4)
        {
            ESP_LOGE( kTag, "send_datagram( ... ): Wrong NetType");
            break;
        }

        if (this->_context == nullptr)
        {
            ESP_LOGE( kTag, "send_datagram( ... ): InterfaceID not found");
            break;
        }

        ip_2_ip4(&client_address)->addr = packet->remoteEp.NetAddr.IPv4.u32[0];

        if (netconn_sendto(this->_socket, buffer, &client_address, packet->remoteEp.NetPort) != ERR_OK)
        {
            ESP_LOGE(kTag, "send_datagram returned %d; Internal Socket Error", errno);
            break;
        }
        success = true;
    }

    netbuf_delete(buffer);
    return success;
}

void LobaroCoap::ReadDatagram()
{
    NetPacket_t  packet;
    struct netbuf *buffer = nullptr;

    if (netconn_recv(this->_socket,  &buffer) != ERR_OK)
        return;

    if (NETCONNTYPE_GROUP( this->_socket->type ) != NETCONN_UDP)
    {
        ESP_LOGE( kTag, "Socket handle is not a datagram type" );
        return;
    }

    if(this->_context == NULL){
        ESP_LOGE( kTag, "Socket handle not associated with any Lobaro CoAP interfaces" );
        return;
    }

    netbuf_data( buffer, (void**) &packet.pData, &packet.size );
    packet.remoteEp.NetPort = buffer->port;
    packet.metaInfo.Type = META_INFO_NONE;

    if( buffer->addr.type == IPADDR_TYPE_V4 )
    {
        packet.remoteEp.NetType = IPV4;
        packet.remoteEp.NetAddr.IPv4.u32[0] = ip_addr_get_ip4_u32( &buffer->addr );

        ESP_LOGI( kTag, "Received %d Bytes from %s:%hu", packet.size, ipaddr_ntoa( &buffer->addr ), buffer->port );
    }
    else if( buffer->addr.type == IPADDR_TYPE_V6 )
    {
        packet.remoteEp.NetType = IPV6;
        packet.remoteEp.NetAddr.IPv6.u32[0] = ip_2_ip6( &buffer->addr )->addr[0];
        packet.remoteEp.NetAddr.IPv6.u32[1] = ip_2_ip6( &buffer->addr )->addr[1];
        packet.remoteEp.NetAddr.IPv6.u32[2] = ip_2_ip6( &buffer->addr )->addr[2];
        packet.remoteEp.NetAddr.IPv6.u32[3] = ip_2_ip6( &buffer->addr )->addr[3];

        ESP_LOGI( kTag, "Received %d Bytes from [%s]:%hu", packet.size, ipaddr_ntoa( &buffer->addr ), buffer->port );
    }

#if LWIP_NETBUF_RECVINFO
    NetEp_t Sender;
    ip_addr_t local_addr; // Don't really care about this address. just wanting the port number
    netconn_getaddr(this->_socket, &local_addr, &Sender.NetPort, 1);

    if( buffer->toaddr.type == IPADDR_TYPE_V4 )
    {
        Sender.NetType = IPV4;
        Sender.NetAddr.IPv4.u32[0] = ip_addr_get_ip4_u32( &buffer->toaddr );

        ESP_LOGI( kTag, "Packet sent to %s:%hu", ipaddr_ntoa( &buffer->toaddr ), Sender.NetPort );
    }
    else if( buffer->addr.type == IPADDR_TYPE_V6 )
    {
        Sender.NetType = IPV6;
        Sender.NetAddr.IPv6.u32[0] = ip_2_ip6( &buffer->toaddr )->addr[0];
        Sender.NetAddr.IPv6.u32[1] = ip_2_ip6( &buffer->toaddr )->addr[1];
        Sender.NetAddr.IPv6.u32[2] = ip_2_ip6( &buffer->toaddr )->addr[2];
        Sender.NetAddr.IPv6.u32[3] = ip_2_ip6( &buffer->toaddr )->addr[3];

        ESP_LOGI( kTag, "Packet sent to [%s]:%hu", ipaddr_ntoa( &buffer->toaddr ), Sender.NetPort );
    }

    if( EpAreEqual( &Sender, &NetEp_IPv4_mulitcast ) || EpAreEqual( &Sender, &NetEp_IPv6_mulitcast ) )
        packet.metaInfo.Type = META_INFO_MULTICAST;

#else
    #error Totally need LWIP_NETBUF_RECVINFO set to 1
#endif /* LWIP_NETBUF_RECVINFO */


    //call the consumer of this socket
    //the packet is only valid during runtime of consuming function!
    //-> so it has to copy relevant data if needed
    // or parse it to a higher level and store this result!
    CoAP_HandleIncomingPacket(this->_context->Handle, &packet);
    netbuf_delete(buffer);

    return;
}

struct LobaroResourceMap
{
    CoAP_Res_t *source;
    LobaroCoapResource *resource;
    LobaroResourceMap *next;
};

LobaroResourceMap *_lobaroResourceMapBase = nullptr;

CoAP_HandlerResult_t LobaroCoapResource::ResourceHandler(CoAP_Message_t *request, CoAP_Message_t *response)
{
    LobaroCoapResource *resource = nullptr;
    for (auto it = _resources.begin(); it != _resources.end(); it++)
    {
        if ((*it)->_resource == request->pResource)
        {
            resource = *it;
            break;
        }
    }
    if (resource == nullptr)
    {
        ESP_LOGE( kTag, "lobaro_requesthandler: mapped resource not found!" );
        response->Code = RESP_INTERNAL_SERVER_ERROR_5_00;
        return HANDLER_ERROR;
    }

    if (resource->_resource == nullptr)
    {
        ESP_LOGE( kTag, "lobaro_requesthandler: mapped resource does not have a callback!" );
        response->Code = RESP_INTERNAL_SERVER_ERROR_5_00;
        return HANDLER_ERROR;
    }

    CoapResult result;
    LobaroCoapMessage wrappedRequest(request), wrappedResponse(response);
    resource->applicationResource->HandleRequest(&wrappedRequest, &wrappedResponse, result);// TODO: pass along these parameters (request, response);
    return result == CoapResult::OK       ? HANDLER_OK :
	       result == CoapResult::Postpone ? HANDLER_POSTPONE :
	                                        HANDLER_ERROR;
}
CoapResource LobaroCoap::CreateResource(IApplicationResource * const applicationResource, const char* uri, CoapResult &result)
{
    // TODO: Figure out subclassing?
    CoapResource resource;
    new (resource.get()) LobaroCoapResource(applicationResource, uri, result);

    if (result != CoapResult::OK)
    {
        // TODO: log error
    }

    return resource;
}

LobaroCoapResource::LobaroCoapResource(IApplicationResource * const applicationResource, const char* uri, CoapResult &result)
    : ICoapResource(applicationResource)
{
    assert(sizeof(*this) <= CoapConstraints::MaxResourceSize);

    CoAP_ResOpts_t resourceOptions =
    {
        (int)CoapContentType::TextPlain,
	    RES_OPT_GET | RES_OPT_PUT | RES_OPT_POST | RES_OPT_DELETE,
        0,
    };

    this->_resource = CoAP_CreateResource((char *)uri, "", resourceOptions, &LobaroCoapResource::ResourceHandler, nullptr);

    if (this->_resource == nullptr)
    {
        //ESP_LOGE( kTag, "CoAP_CreateResource returned null" );
        result = CoapResult::Error;
        return;
    }

    // CoAP_CreateResource errors when AllowedMethods is 0, but 🤷‍
    this->_resource->Options.AllowedMethods = 0;

    _resources.push_back(this);
    result = CoapResult::OK;
}

void LobaroCoapResource::RegisterHandler(CoapMessageCode requestType, CoapResult &result)
{
    switch(requestType)
    {
        case CoapMessageCode::Get:
            this->_resource->Options.AllowedMethods |= RES_OPT_GET;
            break;
        case CoapMessageCode::Post:
            this->_resource->Options.AllowedMethods |= RES_OPT_POST;
            break;
        case CoapMessageCode::Put:
            this->_resource->Options.AllowedMethods |= RES_OPT_PUT;
            break;
        case CoapMessageCode::Delete:
            this->_resource->Options.AllowedMethods |= RES_OPT_DELETE;
            break;
        default:
            result = CoapResult::Error;
            return;
    }
    result = CoapResult::OK;
}

// static CoapResult_t coap_resource_set_contnet_type( CoapResource_t resource, uint16_t content_type )
// {
//     if( resource == NULL ){
//         ESP_LOGE( kTag, "coap_resource_create: resource pointer can not be null" );
//         return kCoapError;
//     }
//     CoAP_Res_t *lobaro_resource = (CoAP_Res_t*)resource;
//     lobaro_resource->Options.Cf = content_type;
//     return kCoapOK;
// }

// static CoapResult_t coap_resource_set_callbakk( CoapResource_t resource, CoapResourceCallback_t callback )
// {
//     lobaro_resource_map_t *resource_map_base = lobaro_resource_map_base;
//     if( resource == NULL ){
//         ESP_LOGE( kTag, "coap_resource_set_callbakk: resource pointer can not be null" );
//         return kCoapError;
//     }
//     while( resource_map_base != NULL )
//     {
//         if( resource_map_base->source == resource )
//             break;
//         resource_map_base = resource_map_base->next;
//     }
//     if( resource_map_base == NULL )
//     {
//         ESP_LOGE( kTag, "coap_resource_set_callbakk: mapped resource not found!" );
//         return kCoapError;
//     }

//     resource_map_base->callback = callback;

//     return kCoapOK;
// }

// static CoapResult_t coap_unregister_reesource( const CoapResource_t resource )
// {
//     ESP_LOGE( kTag, "I'm supposed to unregister something here!" );
//     return kCoapError;
// }

// void LobaroCoapMessage::GetOption_uint( const CoapMessage_t message, const uint16_t option, uint32_t *value )
// {
//     const CoAP_Message_t *pMsg = (CoAP_Message_t*)message;

//     CoAP_option_t *pOpt = CoAP_FindOptionByNumber( (CoAP_Message_t*)pMsg, option );
//     if( pOpt == NULL )
//         return kCoapError;

//     return CoAP_GetUintFromOption( pOpt, value ) == COAP_OK ? kCoapOK : kCoapError;
// }

CoapOption LobaroCoapMessage::GetOption(const uint16_t number, CoapResult &result)
{
    CoapOption option;
    CoapOptionType type = CoapOptionType::Empty;

    CoAP_option_t *opt = CoAP_FindOptionByNumber( this->_message, number);
    if (opt == nullptr)
    {
        result = CoapResult::Error;
        return option;
    }

    for(auto it = std::begin(CoapOptiontypeMap); it != std::end(CoapOptiontypeMap); it++)
    {
        if (std::get<0>(*it) == number)
        {
            type = std::get<1>(*it);
            break;
        }
    }
    switch(type)
    {
        case CoapOptionType::Empty:
            new (option.get()) CoapEmptyOption(number);
            result = CoapResult::OK;
            break;
        case CoapOptionType::Opaque:
            new (option.get()) CoapOpaqueOption(number);
            static_cast<CoapOpaqueOption*>(option.get())->Data.assign(opt->Value, opt->Length);
            result = CoapResult::OK;
            break;
        case CoapOptionType::String:
            new (option.get()) CoapStringOption(number);
            static_cast<CoapStringOption*>(option.get())->Data.assign((char*)opt->Value, opt->Length);
            result = CoapResult::OK;
            break;
        case CoapOptionType::UInt:
            new (option.get()) CoapUIntOption(number);
            CoAP_GetUintFromOption(opt, &static_cast<CoapUIntOption*>(option.get())->Value);
            result = CoapResult::OK;
            break;
        default:
            // Error
            result = CoapResult::Error;
    }
    return option;
}

// static CoapResult_t coap_option_get_next( CoapOption_t* option ){
//     const CoAP_option_t *pOpt = *option;
//     uint16_t optionNumber = pOpt->Number;

//     while(pOpt->next != NULL){
//         pOpt = pOpt->next;

//         if(pOpt->Number == optionNumber){
//             *option = (CoapOption_t)pOpt;
//             return kCoapOK;
//         }
//     }

//     return kCoapError;
// }

// static CoapResult_t coap_option_get_uint( const CoapOption_t option, uint32_t* value )
// {
//     return CoAP_GetUintFromOption( option, value ) == COAP_OK ? kCoapOK : kCoapError;
// }

void LobaroCoapMessage::AddOption(ICoapOption const *option, CoapResult &result)
{
    CoAP_Result_t res;
    if (option->Type == CoapOptionType::UInt)
    {
        auto uintOption = static_cast<CoapUIntOption const *>(option);
        res = CoAP_AppendUintOptionToList(&this->_message->pOptionsList, uintOption->Number, uintOption->Value);
    }
    else
    {
        CoAP_option_t opt;
        opt.Number = option->Number;
        opt.Length = option->GetSize();
        opt.Value = (uint8_t*)option->GetPtr();

        res = CoAP_CopyOptionToList( &this->_message->pOptionsList, &opt);
    }

    result = (res == COAP_OK) ? CoapResult::OK : CoapResult::Error;
}

CoapMessageCode LobaroCoapMessage::GetCode(CoapResult &result) const
{
    result = CoapResult::OK;
    return static_cast<CoapMessageCode>(this->_message->Code);
}

void LobaroCoapMessage::SetCode(CoapMessageCode code, CoapResult &result)
{
    this->_message->Code = static_cast<CoAP_MessageCode_t>(code);
    result = CoapResult::OK;
}

// CoapResult_t coap_message_add_option_uint( CoapMessage_t message, uint16_t option, uint32_t code )
// {
//     CoAP_AppendUintOptionToList( &(((CoAP_Message_t*)message)->pOptionsList), option, code );
//     return kCoapOK;
// }

Payload LobaroCoapMessage::GetPayload(CoapResult &result) const
{
    if (this->_message->Payload == nullptr)
    {
        result = CoapResult::Error;
        return Payload();
    }

    result = CoapResult::OK;
    return Payload(this->_message->Payload, this->_message->PayloadLength);
}

void LobaroCoapMessage::SetPayload(const Payload &payload, CoapResult &result)
{
    auto res = CoAP_SetPayload(this->_message, (uint8_t *)payload.data(), (uint16_t)payload.length(), true);
    result = res == COAP_OK ? CoapResult::OK : CoapResult::Error;
}

void LobaroCoap::TaskHandle(void *pvParameters)
{
    auto instance = static_cast<LobaroCoap *>(pvParameters);
    instance->_socket = netconn_new(NETCONN_UDP);

    if (instance->_socket == nullptr)
    {
        ESP_LOGE( kTag, "netconn_new(): Failed to get new socket" );
        vTaskDelete(nullptr);
        return;
    }

    // Allocate a socket in Lobaro CoAP's memory and return the address
    if ((instance->_context = CoAP_NewSocket(instance)) == nullptr)
    {
        ESP_LOGE(kTag, "CoAP_NewSocket(): failed socket allocation");
        netconn_delete(instance->_socket);
        vTaskDelete(nullptr);
        return;
    }

    if (netconn_bind(instance->_socket, nullptr, kCoapPort) != ERR_OK)
    {
        ESP_LOGE(kTag, "netconn_bind( ... ): Failed");
        netconn_delete(instance->_socket);
        vTaskDelete(nullptr);
        return;
    }

    ip_addr_t multicast_addr;
    IP4_ADDR( ip_2_ip4(&multicast_addr) , 224, 0, 1, 187 );
    if (netconn_join_leave_group(instance->_socket, &multicast_addr, nullptr, NETCONN_JOIN ) != ERR_OK)
        ESP_LOGE( kTag, "netconn_join_leave_group( ... ): Failed" );

    ESP_LOGI( kTag, "Coap library now listening" );

    //user callback registration
    instance->_context->Tx = &LobaroCoap::SendDatagram;
    instance->_context->Alive = true;

    ESP_LOGD( kTag, "Listening: Port: %hu", kCoapPort);

    for(;;) {
        // TODO: Loop through all interfaces, and perform interface specific polling stuff
        // For now, work with UDP sockets and call Lobaro-coap stuff

        if (instance->_context == nullptr)
            break;

        netconn_set_recvtimeout(instance->_socket, 10);

        instance->ReadDatagram();

        CoAP_doWork();
    }

    vTaskDelete(nullptr);
}

bool LobaroCoap::SendDatagram(SocketHandle_t socketHandle, NetPacket_t *packet)
{
    auto instance = static_cast<LobaroCoap *>(socketHandle);
    return instance->SendDatagram(packet);
}

static void hal_uart_puts( char *s ) {
    ESP_LOGD( kTag, "%s", s );
}

static TimerHandle_t _timerHandle;
static uint64_t _seconds = 0;

static void timer_handle_callback( TimerHandle_t xTimer ){
    _seconds++;
}

static uint32_t hal_rtc_1Hz_Cnt( void )
{
    if (_timerHandle == nullptr) {
        ESP_LOGI( kTag, "Creating timer" );
        _timerHandle = xTimerCreate( "Lobaro CoAP", pdMS_TO_TICKS( 1000 ) , true, nullptr, &timer_handle_callback );
        assert(_timerHandle != nullptr);
    }

    if( xTimerIsTimerActive(_timerHandle) == false)
        xTimerStart(_timerHandle, 0);

    return _seconds;
}
