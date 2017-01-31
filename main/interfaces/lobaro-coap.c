#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include "lwip/sockets.h"

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

static struct fd_set readfds;


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
    NetSocket_t *pSocket;
    struct sockaddr_in client_address;

    if(pckt->Receiver.NetType != IPV4){
        ESP_LOGE( TAG, "send_datagram( ... ): Wrong NetType");
        return false;
    }

    pSocket = RetrieveSocket2( ifID );
    if( pSocket == NULL )
    {
        ESP_LOGE( TAG, "send_datagram( ... ): InterfaceID not found");
        return false;
    }

    memset( &client_address, 0, sizeof( client_address ) );
    client_address.sin_len = sizeof( client_address );
    client_address.sin_family = AF_INET;
    client_address.sin_port = htons( pckt->Receiver.NetPort );
    client_address.sin_addr.s_addr = htonl( pckt->Receiver.NetAddr.IPv4.u32[0] );

    if( sendto( (int) pSocket->Handle, pckt->pData, pckt->size, 0, (struct sockaddr*) &client_address, client_address.sin_len ) >= 0 )
        return true;

    ESP_LOGE( TAG, "send_datagram returned %d; Internal Socket Error", errno );
    return false;
}

static uint8_t coap_payload[ MAX_PAYLOAD_SIZE ];

static void read_datagram( int fd )
{
    struct sockaddr_storage remote_addr = { 0 };
    char address[ INET6_ADDRSTRLEN ];
    int ret;

    NetPacket_t  packet;
    NetSocket_t* pSocket = NULL;
    
    socklen_t remote_addr_len = sizeof( remote_addr );
    if( ( ret = recvfrom( fd, coap_payload, MAX_PAYLOAD_SIZE, 0, (struct sockaddr*) &remote_addr, &remote_addr_len ) ) <= 0 )
    {
        ESP_LOGE( TAG, "recvfrom() failed with %d", ret );
        return;
    }

    pSocket = RetrieveSocket( (SocketHandle_t) fd );

    if(pSocket == NULL){
        ESP_LOGE( TAG, "Socket handle not associated with any Lobaro CoAP interfaces" );
        return;
    }

    packet.pData = coap_payload;
    packet.size = ret;

    inet_ntop( remote_addr.ss_family, &remote_addr, address, INET6_ADDRSTRLEN );
    if( remote_addr.ss_family == AF_INET )
    {
        packet.Sender.NetType = IPV4;
        packet.Sender.NetPort = ntohs( ( (struct sockaddr_in*) &remote_addr )->sin_port );
        packet.Sender.NetAddr.IPv4.u32[0] = ntohl( ( (struct sockaddr_in*) &remote_addr )->sin_addr.s_addr );

        ESP_LOGI( TAG, "Received %d Bytes from %s:%hu", ret, address, packet.Sender.NetPort );
    }
    else
    {
        packet.Sender.NetType = IPV6;
        packet.Sender.NetPort = ntohs( ( (struct sockaddr_in6*) &remote_addr )->sin6_port );
        packet.Sender.NetAddr.IPv6.u32[0] = ntohl( ( (struct sockaddr_in6*) &remote_addr )->sin6_addr.un.u32_addr[0] );
        packet.Sender.NetAddr.IPv6.u32[1] = ntohl( ( (struct sockaddr_in6*) &remote_addr )->sin6_addr.un.u32_addr[1] );
        packet.Sender.NetAddr.IPv6.u32[2] = ntohl( ( (struct sockaddr_in6*) &remote_addr )->sin6_addr.un.u32_addr[2] );
        packet.Sender.NetAddr.IPv6.u32[3] = ntohl( ( (struct sockaddr_in6*) &remote_addr )->sin6_addr.un.u32_addr[3] );

        ESP_LOGI( TAG, "Received %d Bytes from [%s]:%hu", ret, address, ( (struct sockaddr_in6*) &remote_addr )->sin6_port );
    }

    coap_memcpy( &pSocket->EpLocal, &packet.Receiver, sizeof( NetEp_t ) );

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
    int listen_socket;

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

    listen_socket = socket( AF_INET, SOCK_DGRAM, 0 );
    if( listen_socket < 0 )
    {
        ESP_LOGE( TAG, "socket( ... ): Failed to get new socket" );
        return -1;
    }

    int on = 1;
    if ( setsockopt( listen_socket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof( on ) ) < 0 )
        ESP_LOGE( TAG , "setsockopt SO_REUSEADDR");

    {
        struct sockaddr_in listen_address;

        memset( &listen_address, 0, sizeof( struct sockaddr_in ) );
        listen_address.sin_family = AF_INET;
        listen_address.sin_len = sizeof( listen_address );
        listen_address.sin_addr.s_addr = INADDR_ANY;
        listen_address.sin_port = htons( COAP_PORT );

        if( bind( listen_socket, (struct sockaddr*) &listen_address, sizeof( listen_address ) ) < 0 )
        {
            ESP_LOGE( TAG, "bind( ... ): Failed" );
            close( listen_socket );
            return NULL;
        }

        ESP_LOGI( TAG, "Coap library now listening" );
    }

    pSocket->Handle = (SocketHandle_t) listen_socket; // external  to CoAP Stack
    pSocket->ifID = COAP_INTERFACE_CLEARTEXT;         // internal  to CoAP Stack

    //user callback registration
    pSocket->RxCB = CoAP_onNewPacketHandler;
    pSocket->Tx = &send_datagram;
    pSocket->Alive = true;

    ESP_LOGD( TAG, "Listening: IfID: %d, socket: %d Port: %hu", COAP_INTERFACE_CLEARTEXT, listen_socket, COAP_PORT );

    return 0;
}

void lobaro_coap_do_work( void )
{
    struct timeval tv = (struct timeval) {
        .tv_sec = 0,
        .tv_usec = 250000
    };

    // TODO: Loop through all interfaces, and perform interface specific polling stuff
    // For now, work with UDP sockets and call Lobaro-coap stuff

    NetSocket_t *pSocket = RetrieveSocket2( COAP_INTERFACE_CLEARTEXT );
    if( pSocket == NULL )
        return;

    int listen_socket = (int) pSocket->Handle;

    FD_ZERO( &readfds );
    FD_CLR( listen_socket, &readfds );
    FD_SET( listen_socket, &readfds );

    int ret = select( FD_SETSIZE, &readfds, 0, 0, &tv );
    if( ret > 0 )
    {
        ESP_LOGD( TAG, "Oop, something happened~!" );

        if( FD_ISSET( listen_socket, &readfds ) )
            read_datagram( listen_socket );
    }

    CoAP_doWork();
}