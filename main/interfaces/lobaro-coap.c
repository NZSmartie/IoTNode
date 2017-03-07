#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include "lwip/api.h"

#include "lobaro-coap/coap.h"

#include "lobaro-coap.h"

#ifdef LOG_LOCAL_LEVEL
    #undef LOG_LOCAL_LEVEL
#endif 

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include "esp_log.h"

static const char* TAG = "Lobaro-CoAP";

static const unsigned short COAP_PORT = 5683;
static const unsigned short COAP_PORT_DTLS = 5684;

#define COAP_MEMORY_SIZE 4096

static const uint8_t COAP_INTERFACE_CLEARTEXT = 0;
static const uint8_t COAP_INTERFACE_ENCRYPTED = 1;
static const uint8_t COAP_INTERFACE_BROADCAST = 2;

typedef struct
{
    void *inHandle;
    void (*recv_dataggram)( void );
    void (*send_dataggram)( void );
} LobaroCoapSocket_t;

//Uart/Display function to print debug/status messages to
void hal_uart_puts( char *s ) {
    ESP_LOGD( TAG, "%s", s );
}

void hal_uart_putc( char c ){
    ESP_LOGD( TAG, "%c", c );
}

static TimerHandle_t timer_handle = NULL;
uint64_t seconds = 0;

static void timer_handle_callback( TimerHandle_t xTimer ){
    seconds++;
}

//1Hz Clock used by timeout logic
uint32_t hal_rtc_1Hz_Cnt( void ){
    if( timer_handle == NULL ) {
        timer_handle = xTimerCreate( "Lobaro CoAP", pdMS_TO_TICKS( 1000 ) , pdTRUE, (void *) 0, timer_handle_callback );
        assert( timer_handle != NULL );
    }

    if( xTimerIsTimerActive( timer_handle ) == pdFALSE )
        xTimerStart( timer_handle, 0 );

    return seconds;
}

//Non volatile memory e.g. flash/sd-card/eeprom
//used to store observers during deepsleep of server
uint8_t* hal_nonVolatile_GetBufPtr(){
    return NULL;
}

bool hal_nonVolatile_WriteBuf( uint8_t* data, uint32_t len ){
    return false;
}

static bool send_datagram( uint8_t ifID, NetPacket_t* pckt )
{
    bool success = false;
    NetSocket_t *pSocket;
    ip_addr_t client_address = IPADDR4_INIT( 0 );
    struct netbuf *buffer = netbuf_new();

    do
    {
        if( netbuf_ref(buffer, pckt->pData, pckt->size) != ERR_OK)
        {
            ESP_LOGE( TAG, "netbuf_ref( ... ): failed");
            break;
        }

        if(pckt->Receiver.NetType != IPV4){
            ESP_LOGE( TAG, "send_datagram( ... ): Wrong NetType");
            break;
        }

        pSocket = RetrieveSocket2( ifID );
        if( pSocket == NULL )
        {
            ESP_LOGE( TAG, "send_datagram( ... ): InterfaceID not found");
            break;
        }

        ip_2_ip4(&client_address)->addr = pckt->Receiver.NetAddr.IPv4.u32[0];

        if( netconn_sendto( (struct netconn*) pSocket->Handle, buffer, &client_address, pckt->Receiver.NetPort ) != ERR_OK )
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
    NetSocket_t* pSocket = NULL;
    
    pSocket = RetrieveSocket( (SocketHandle_t) fd );

    if(pSocket == NULL){
        ESP_LOGE( TAG, "Socket handle not associated with any Lobaro CoAP interfaces" );
        return;
    }

    netbuf_data( buffer, (void**) &packet.pData, &packet.size );
    packet.Sender.NetPort = buffer->port;
    packet.Receiver.NetPort = pSocket->EpLocal.NetPort;

    if( buffer->addr.type == IPADDR_TYPE_V4 )
    {
        packet.Sender.NetType = IPV4;
        packet.Sender.NetAddr.IPv4.u32[0] = ip_addr_get_ip4_u32( &buffer->addr );
     
        ESP_LOGI( TAG, "Received %d Bytes from %s:%hu", packet.size, ipaddr_ntoa( &buffer->addr ), buffer->port );
    }
    else if( buffer->addr.type == IPADDR_TYPE_V6 )
    {
        packet.Sender.NetType = IPV6;
        packet.Sender.NetAddr.IPv6.u32[0] = ip_2_ip6( &buffer->addr )->addr[0];
        packet.Sender.NetAddr.IPv6.u32[1] = ip_2_ip6( &buffer->addr )->addr[1];
        packet.Sender.NetAddr.IPv6.u32[2] = ip_2_ip6( &buffer->addr )->addr[2];
        packet.Sender.NetAddr.IPv6.u32[3] = ip_2_ip6( &buffer->addr )->addr[3];

        ESP_LOGI( TAG, "Received %d Bytes from [%s]:%hu", packet.size, ipaddr_ntoa( &buffer->addr ), buffer->port );
    }

#if LWIP_NETBUF_RECVINFO
    // packet.Receiver.NetPort = buffer->port;

    if( buffer->toaddr.type == IPADDR_TYPE_V4 )
    {
        packet.Receiver.NetType = IPV4;
        packet.Receiver.NetAddr.IPv4.u32[0] = ip_addr_get_ip4_u32( &buffer->toaddr );
     
        ESP_LOGI( TAG, "Packet sent to %s", ipaddr_ntoa( &buffer->toaddr ) );
    }
    else if( buffer->addr.type == IPADDR_TYPE_V6 )
    {
        packet.Receiver.NetType = IPV6;
        packet.Receiver.NetAddr.IPv6.u32[0] = ip_2_ip6( &buffer->toaddr )->addr[0];
        packet.Receiver.NetAddr.IPv6.u32[1] = ip_2_ip6( &buffer->toaddr )->addr[1];
        packet.Receiver.NetAddr.IPv6.u32[2] = ip_2_ip6( &buffer->toaddr )->addr[2];
        packet.Receiver.NetAddr.IPv6.u32[3] = ip_2_ip6( &buffer->toaddr )->addr[3];

        ESP_LOGI( TAG, "Packet sent to [%s]", ipaddr_ntoa( &buffer->toaddr ) );
    }
#else
    #error Totally need LWIP_NETBUF_RECVINFO set to 1
#endif /* LWIP_NETBUF_RECVINFO */

    packet.MetaInfo.Type = META_INFO_NONE;

    //call the consumer of this socket
    //the packet is only valid during runtime of consuming function!
    //-> so it has to copy relevant data if needed
    // or parse it to a higher level and store this result!
    pSocket->RxCB(pSocket->ifID, &packet);

    return;
}

int lobaro_coap_init( void )
{
    static uint8_t coap_work_memory[ COAP_MEMORY_SIZE ]; // Working memory of CoAPs internal memory allocator
    NetSocket_t* pSocket;
    struct netconn* listen_socket;

    CoAP_Init( coap_work_memory, COAP_MEMORY_SIZE );

    // Check to see if the interface id is already in use
    if( RetrieveSocket2( 0 ) != NULL ) {
        ESP_LOGE( TAG, "RetrieveSocket2() failed, Interface ID already in use" );
        return NULL;
    }

    // Allocate a socket in Lobaro CoAP's memory and return the address
    if( ( pSocket = AllocSocket() ) == NULL ){
        ESP_LOGE( TAG, "AllocSocket(): failed socket allocation" );
        return NULL;
    }

    //local side of socket
    pSocket->EpLocal.NetType = IPV4;
    pSocket->EpLocal.NetPort = COAP_PORT;

    listen_socket = netconn_new(NETCONN_UDP);
    if( listen_socket == NULL )
    {
        ESP_LOGE( TAG, "netconn_new(): Failed to get new socket" );
        return -1;
    }

    ip_addr_t multicast_addr;

    // if ( setsockopt( listen_socket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof( on ) ) < 0 )
    //     ESP_LOGE( TAG , "setsockopt SO_REUSEADDR");

    if( netconn_bind( listen_socket, NULL, COAP_PORT ) != ERR_OK )
    {
        ESP_LOGE( TAG, "netconn_bind( ... ): Failed" );
        netconn_delete( listen_socket );
        return NULL;
    }
    
    IP4_ADDR( ip_2_ip4(&multicast_addr) , 224, 0, 1, 187 );
    if( netconn_join_leave_group( listen_socket, &multicast_addr, NULL, NETCONN_JOIN ) != ERR_OK )
        ESP_LOGE( TAG, "netconn_join_leave_group( ... ): Failed" );

    ESP_LOGI( TAG, "Coap library now listening" );

    pSocket->Handle = (SocketHandle_t) listen_socket; // external  to CoAP Stack
    pSocket->ifID = COAP_INTERFACE_CLEARTEXT;         // internal  to CoAP Stack

    //user callback registration
    pSocket->RxCB = CoAP_onNewPacketHandler;
    pSocket->Tx = &send_datagram;
    pSocket->Alive = true;

    ESP_LOGD( TAG, "Listening: IfID: %d, Port: %hu", COAP_INTERFACE_CLEARTEXT, COAP_PORT );

    return 0;
}

void lobaro_coap_do_work( void )
{
    // TODO: Loop through all interfaces, and perform interface specific polling stuff
    // For now, work with UDP sockets and call Lobaro-coap stuff

    NetSocket_t *pSocket = RetrieveSocket2( COAP_INTERFACE_CLEARTEXT );
    if( pSocket == NULL )
        return;

    struct netconn* listen_socket = (struct netconn*) pSocket->Handle;
    struct netbuf* buffer = NULL;

    listen_socket->recv_timeout = 0;
    int ret = netconn_recv( listen_socket,  &buffer );
    
    if( ret == ERR_OK )
    {
        ESP_LOGD( TAG, "Oop, something happened~!" );
        read_datagram( listen_socket, buffer );
        netbuf_delete( buffer );
    }

    CoAP_doWork();
}