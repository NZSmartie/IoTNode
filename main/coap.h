#ifndef __MAIN_COAP_
#define __MAIN_COAP_

#define COAP_THREAD_NAME "coap"
#define COAP_THREAD_STACK_SIZE_WORDS 10240
#define COAP_THREAD_PRIORITY 8

extern const int COAP_CONNECTED_BIT;

extern const int OPENSSL_DEMO_RECV_BUF_LEN;
extern const char *LOCAL_PORT;

void CoAP_Init( EventGroupHandle_t wifi_events );
void CoAP_Thread( void* p );

#endif /* __MAIN_COAP_ */