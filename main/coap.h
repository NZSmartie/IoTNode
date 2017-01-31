#ifndef __MAIN_COAP_
#define __MAIN_COAP_

#define COAP_THREAD_NAME "coap"
#define COAP_THREAD_STACK_SIZE_WORDS 10240
#define COAP_THREAD_PRIORITY 8

typedef struct 
{
    const unsigned char *cert_ptr;
    size_t cert_len;

    const unsigned char *cert_key_ptr;
    size_t cert_key_len;
} CoapDtlsOptions_t;

typedef struct {
    struct {
        unsigned char useDTLS: 1;
    } flags;
    CoapDtlsOptions_t DTLS;
} CoapOptions_t;

extern const int COAP_CONNECTED_BIT;

void coap_init( const CoapOptions_t *options, EventGroupHandle_t wifi_events );

#endif /* __MAIN_COAP_ */