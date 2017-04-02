#include "assert.h"
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include "lwip/api.h"

#include "liblobaro_coap.h"
#include "coap_options.h"
#include "option-types/coap_option_cf.h"
#include "interface/network/net_Endpoint.h"

#include "lobaro-coap.h"

#ifdef LOG_LOCAL_LEVEL
    #undef LOG_LOCAL_LEVEL
#endif 

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include "esp_log.h"

// Custom Resources

#include "../resources/name.h"

static const char* TAG = "Lobaro-CoAP";

static const unsigned short COAP_PORT = 5683;
static const unsigned short COAP_PORT_DTLS = 5684;

#define COAP_MEMORY_SIZE 4096
static uint8_t coap_work_memory[ COAP_MEMORY_SIZE ]; // Working memory of CoAPs internal memory allocator

CoAP_Config_t coap_cfg = {
    .Memory = coap_work_memory,
    .MemorySize = COAP_MEMORY_SIZE
};

static const uint8_t COAP_INTERFACE_CLEARTEXT = 0;
static const uint8_t COAP_INTERFACE_ENCRYPTED = 1;
static const uint8_t COAP_INTERFACE_MULTICAST = 2;

static CoAP_Socket_t *pSocket = NULL;

//Uart/Display function to print debug/status messages to
void hal_uart_puts( char *s ) {
    ESP_LOGD( TAG, "%s", s );
}

static TimerHandle_t timer_handle = NULL;
uint64_t seconds = 0;

static void timer_handle_callback( TimerHandle_t xTimer ){
    seconds++;
}

//1Hz Clock used by timeout logic
uint32_t hal_rtc_1Hz_Cnt( void )
{
    if( timer_handle == NULL ) {
        ESP_LOGI( TAG, "Creating timer" );
        timer_handle = xTimerCreate( "Lobaro CoAP", pdMS_TO_TICKS( 1000 ) , pdTRUE, (void *) 0, timer_handle_callback );
        assert( timer_handle != NULL );
    }

    if( xTimerIsTimerActive( timer_handle ) == pdFALSE )
        xTimerStart( timer_handle, 0 );

    return seconds;
}

static CoAP_API_t coap_api = {
    .rtc1HzCnt = hal_rtc_1Hz_Cnt,
    .debugPuts = hal_uart_puts,
};

static bool send_datagram(SocketHandle_t socketHandle, NetPacket_t* pckt)
{
    bool success = false;
    ip_addr_t client_address = IPADDR4_INIT( 0 );

    if( NETCONNTYPE_GROUP( ( (struct netconn*) socketHandle)->type ) != NETCONN_UDP )
    {
        ESP_LOGE( TAG, "Socket handle is not a datagram type" );
        return false;
    }

    struct netbuf *buffer = netbuf_new();

    do
    {
        if( netbuf_ref( buffer, pckt->pData, pckt->size ) != ERR_OK)
        {
            ESP_LOGE( TAG, "netbuf_ref( ... ): failed");
            break;
        }

        if( pckt->remoteEp.NetType != IPV4 ){
            ESP_LOGE( TAG, "send_datagram( ... ): Wrong NetType");
            break;
        }

        if( pSocket == NULL )
        {
            ESP_LOGE( TAG, "send_datagram( ... ): InterfaceID not found");
            break;
        }

        ip_2_ip4( &client_address )->addr = pckt->remoteEp.NetAddr.IPv4.u32[0];

        if( netconn_sendto( (struct netconn*) socketHandle, buffer, &client_address, pckt->remoteEp.NetPort ) != ERR_OK )
        {
            ESP_LOGE( TAG, "send_datagram returned %d; Internal Socket Error", errno );
            break;
        }
        success = true;
    }
    while( 0 );

    netbuf_delete( buffer );
    return success;
}

static void read_datagram( struct netconn* fd, struct netbuf* buffer )
{
    NetPacket_t  packet;
    
    if( NETCONNTYPE_GROUP( fd->type ) != NETCONN_UDP )
    {
        ESP_LOGE( TAG, "Socket handle is not a datagram type" );
        return;
    }

    if(pSocket == NULL){
        ESP_LOGE( TAG, "Socket handle not associated with any Lobaro CoAP interfaces" );
        return;
    }

    netbuf_data( buffer, (void**) &packet.pData, &packet.size );
    packet.remoteEp.NetPort = buffer->port;
    packet.metaInfo.Type = META_INFO_NONE;

    if( buffer->addr.type == IPADDR_TYPE_V4 )
    {
        packet.remoteEp.NetType = IPV4;
        packet.remoteEp.NetAddr.IPv4.u32[0] = ip_addr_get_ip4_u32( &buffer->addr );
     
        ESP_LOGI( TAG, "Received %d Bytes from %s:%hu", packet.size, ipaddr_ntoa( &buffer->addr ), buffer->port );
    }
    else if( buffer->addr.type == IPADDR_TYPE_V6 )
    {
        packet.remoteEp.NetType = IPV6;
        packet.remoteEp.NetAddr.IPv6.u32[0] = ip_2_ip6( &buffer->addr )->addr[0];
        packet.remoteEp.NetAddr.IPv6.u32[1] = ip_2_ip6( &buffer->addr )->addr[1];
        packet.remoteEp.NetAddr.IPv6.u32[2] = ip_2_ip6( &buffer->addr )->addr[2];
        packet.remoteEp.NetAddr.IPv6.u32[3] = ip_2_ip6( &buffer->addr )->addr[3];

        ESP_LOGI( TAG, "Received %d Bytes from [%s]:%hu", packet.size, ipaddr_ntoa( &buffer->addr ), buffer->port );
    }

#if LWIP_NETBUF_RECVINFO
    NetEp_t Sender;
    ip_addr_t local_addr; // Don't really care about this address. jsut wanting the port number
    netconn_getaddr( fd, &local_addr, &Sender.NetPort, 1);

    if( buffer->toaddr.type == IPADDR_TYPE_V4 )
    {
        Sender.NetType = IPV4;
        Sender.NetAddr.IPv4.u32[0] = ip_addr_get_ip4_u32( &buffer->toaddr );
     
        ESP_LOGI( TAG, "Packet sent to %s:%hu", ipaddr_ntoa( &buffer->toaddr ), Sender.NetPort );
    }
    else if( buffer->addr.type == IPADDR_TYPE_V6 )
    {
        Sender.NetType = IPV6;
        Sender.NetAddr.IPv6.u32[0] = ip_2_ip6( &buffer->toaddr )->addr[0];
        Sender.NetAddr.IPv6.u32[1] = ip_2_ip6( &buffer->toaddr )->addr[1];
        Sender.NetAddr.IPv6.u32[2] = ip_2_ip6( &buffer->toaddr )->addr[2];
        Sender.NetAddr.IPv6.u32[3] = ip_2_ip6( &buffer->toaddr )->addr[3];

        ESP_LOGI( TAG, "Packet sent to [%s]:%hu", ipaddr_ntoa( &buffer->toaddr ), Sender.NetPort );
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
    CoAP_HandleIncomingPacket(pSocket->Handle, &packet);

    return;
}

void lobaro_coap_init( void )
{
    CoAP_Init( coap_api, coap_cfg );

    coap_create_resources();
}

int lobaro_coap_listen( void )
{
    struct netconn* listen_socket;

    listen_socket = netconn_new(NETCONN_UDP);
    if( listen_socket == NULL )
    {
        ESP_LOGE( TAG, "netconn_new(): Failed to get new socket" );
        return -1;
    }

    // Allocate a socket in Lobaro CoAP's memory and return the address
    if( ( pSocket = CoAP_NewSocket(listen_socket) ) == NULL ){
        ESP_LOGE( TAG, "CoAP_NewSocket(): failed socket allocation" );
        netconn_delete(listen_socket);
        return NULL;
    }

    if( netconn_bind( listen_socket, NULL, COAP_PORT ) != ERR_OK )
    {
        ESP_LOGE( TAG, "netconn_bind( ... ): Failed" );
        netconn_delete( listen_socket );
        return NULL;
    }
    
    ip_addr_t multicast_addr;
    IP4_ADDR( ip_2_ip4(&multicast_addr) , 224, 0, 1, 187 );
    if( netconn_join_leave_group( listen_socket, &multicast_addr, NULL, NETCONN_JOIN ) != ERR_OK )
        ESP_LOGE( TAG, "netconn_join_leave_group( ... ): Failed" );

    ESP_LOGI( TAG, "Coap library now listening" );

    //user callback registration
    pSocket->Tx = &send_datagram;
    pSocket->Alive = true;

    ESP_LOGD( TAG, "Listening: IfID: %d, Port: %hu", COAP_INTERFACE_CLEARTEXT, COAP_PORT );

    return 0;
}

void lobaro_coap_do_work( void )
{
    // TODO: Loop through all interfaces, and perform interface specific polling stuff
    // For now, work with UDP sockets and call Lobaro-coap stuff

    if( pSocket == NULL ){
        return;
    }

    struct netconn* listen_socket = (struct netconn*) pSocket->Handle;
    struct netbuf* buffer = NULL;

    netconn_set_recvtimeout( listen_socket, 10 );
    int ret = netconn_recv( listen_socket,  &buffer );
    
    if( ret == ERR_OK )
    {
        ESP_LOGD( TAG, "Oop, something happened~!" );
        read_datagram( listen_socket, buffer );
        netbuf_delete( buffer );
    }

    CoAP_doWork();
}

typedef struct lobaro_resource_map
{
    CoAP_Res_t *source;
    CoapResourceCallback_t callback;
    struct lobaro_resource_map *next;
} lobaro_resource_map_t;

lobaro_resource_map_t *lobaro_resource_map_base = NULL;

static CoAP_HandlerResult_t lobaro_requesthandler(CoAP_Message_t* pReq, CoAP_Message_t* pResp, CoAP_Res_t* pResource)
{
    lobaro_resource_map_t *resource_map_base = lobaro_resource_map_base;
    while( resource_map_base != NULL ){
        if( resource_map_base->source == pResource )
            break;
        resource_map_base = resource_map_base->next;
    }
    if( resource_map_base == NULL )
    {
        ESP_LOGE( TAG, "lobaro_requesthandler: mapped resource not found!" );
        pResp->Code = RESP_INTERNAL_SERVER_ERROR_5_00;
        return HANDLER_ERROR;
    }

    if( resource_map_base->callback == NULL ){
        ESP_LOGE( TAG, "lobaro_requesthandler: mapped resource does not have a callback!" );
        pResp->Code = RESP_INTERNAL_SERVER_ERROR_5_00;
        return HANDLER_ERROR;
    }

    CoapResult_t res = resource_map_base->callback( (const CoapResource_t)pResource, (const CoapMessage_t)pReq, (CoapMessage_t)pResp );
    return res == kCoapOK       ? HANDLER_OK :
	       res == kCoapPostpone ? HANDLER_POSTPONE : 
	                              HANDLER_ERROR;
}

static CoapResult_t coap_resource_create( CoapResource_t *resource, const char* uri )
{
    if( resource == NULL ){
        ESP_LOGE( TAG, "coap_resource_create: resource pointer can not be null" );
        return kCoapError;    
    }

    lobaro_resource_map_t *resource_map = malloc( sizeof( lobaro_resource_map_t ) ), 
                          **resource_map_base = &lobaro_resource_map_base;

    *resource = CoAP_CreateResource( uri, "", (CoAP_ResOpts_t) {
            .Cf = 0, 
            .AllowedMethods = RES_OPT_GET | RES_OPT_PUT | RES_OPT_POST | RES_OPT_DELETE 
        }, lobaro_requesthandler, NULL );

    if( *resource == NULL )
    {
        free( resource_map );
        ESP_LOGE( TAG, "CoAP_CreateResource returned null" );
        return kCoapError;
    }

    resource_map->source = *resource;
    resource_map->callback = NULL;
    resource_map->next = NULL;

    if( *resource_map_base == NULL )
        *resource_map_base = resource_map;
    else{
        while( (*resource_map_base)->next != NULL )
            resource_map_base = &(*resource_map_base)->next;
        (*resource_map_base)->next = resource_map;
    }

    
    return kCoapOK;
}

static CoapResult_t coap_resource_set_contnet_type( CoapResource_t resource, uint16_t content_type )
{
    if( resource == NULL ){
        ESP_LOGE( TAG, "coap_resource_create: resource pointer can not be null" );
        return kCoapError;    
    }
    CoAP_Res_t *lobaro_resource = (CoAP_Res_t*)resource;
    lobaro_resource->Options.Cf = content_type;
    return kCoapOK;
}

static CoapResult_t coap_resource_set_callbakk( CoapResource_t resource, CoapResourceCallback_t callback )
{
    lobaro_resource_map_t *resource_map_base = lobaro_resource_map_base;
    if( resource == NULL ){
        ESP_LOGE( TAG, "coap_resource_set_callbakk: resource pointer can not be null" );
        return kCoapError;    
    }
    while( resource_map_base != NULL ) 
    {
        if( resource_map_base->source == resource )
            break;
        resource_map_base = resource_map_base->next;
    }
    if( resource_map_base == NULL )
    {
        ESP_LOGE( TAG, "coap_resource_set_callbakk: mapped resource not found!" );
        return kCoapError;
    }

    resource_map_base->callback = callback;

    return kCoapOK;
}

static CoapResult_t coap_unregister_reesource( const CoapResource_t resource )
{
    ESP_LOGE( TAG, "I'm supposed to unregister something here!" );
    return kCoapError;
}

static CoapResult_t coap_message_get_option_uint( const CoapMessage_t message, const uint16_t option, uint32_t *value )
{
    const CoAP_Message_t *pMsg = (CoAP_Message_t*)message;

    CoAP_option_t *pOpt = CoAP_FindOptionByNumber( (CoAP_Message_t*)pMsg, option );
    if( pOpt == NULL )
        return kCoapError;

    *value = CoAP_GetAcceptOptionVal( pOpt );
    return kCoapOK;
}

static CoapResult_t coap_message_get_code( const CoapMessage_t message, uint8_t *code )
{
    if( code == NULL )
    {
        ESP_LOGE( TAG, "coap_message_get_code: code is nulll" );
        return kCoapError;
    }
    *code = ((CoAP_Message_t*)message)->Code;
    return kCoapOK;
}

static CoapResult_t coap_message_set_code( CoapMessage_t message, uint8_t code )
{
    ((CoAP_Message_t*)message)->Code = code;
    return kCoapOK;
}

CoapResult_t coap_message_add_option_uint( CoapMessage_t message, uint16_t option, uint32_t code )
{
    CoAP_AppendUintOptionToList( &(((CoAP_Message_t*)message)->pOptionsList), option, code );
    return kCoapOK;
}

CoapResult_t coap_message_get_payload( const CoapMessage_t message, uint8_t **payload, size_t *length )
{
    *payload = ((CoAP_Message_t*)message)->Payload;
    if( length != NULL )
        *length = ((CoAP_Message_t*)message)->PayloadLength;
    return kCoapOK;
}

CoapResult_t coap_message_set_payload( CoapMessage_t message, uint8_t *payload, size_t length )
{
    CoAP_SetPayload( (CoAP_Message_t*)message, payload, length, true );
    return kCoapOK;
}

__attribute__((const)) CoapInterface_t CoapGetInterface( void )
{
    return (CoapInterface_t){
        .message_get_option_uint   = coap_message_get_option_uint,
        .message_add_option_uint   = coap_message_add_option_uint,
        .message_set_code          = coap_message_set_code,
        .message_get_code          = coap_message_get_code,
        .message_get_payload       = coap_message_get_payload,
        .message_set_payload       = coap_message_set_payload,
        .resource_create           = coap_resource_create,
        .resource_set_contnet_type = coap_resource_set_contnet_type,
        .resource_set_callbakk     = coap_resource_set_callbakk,
        .unregister_reesource      = coap_unregister_reesource,
    };
}