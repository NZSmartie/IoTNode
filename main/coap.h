#ifndef __MAIN_COAP_
#define __MAIN_COAP_

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#define COAP_THREAD_NAME "coap"
#define COAP_THREAD_STACK_SIZE_WORDS 10240
#define COAP_THREAD_PRIORITY 8

typedef enum 
{
    kCoapOK = 0,
    kCoapError,
    kCoapPostpone,
} CoapResult_t;

enum CoapContentType {
    kCoapContentTypeTextPlain = 0,
	kCoapContentTypeLinkFormat = 40,
	kCoapContentTypeApplicationXml = 41,
	kCoapContentTypeApplicationOctetStream = 42,
	kCoapContentTypeApplicationExi = 47,
	kCoapContentTypeApplicationJson = 50,
	kCoapContentTypeApplicationCbor = 60
};

enum CoapOption {
    kCoapOptionIfMatch = 1,
    kCoapOptionUriHost = 3,
    kCoapOptionETag = 4,
    kCoapOptionIfNoneMatch = 5,
    kCoapOptionUriPort = 7,
    kCoapOptionLocationPath = 8,
    kCoapOptionUriPath = 11,
    kCoapOptionContentFormat = 12,
    kCoapOptionMaxAge = 14,
    kCoapOptionUriQuery = 15,
    kCoapOptionAccept = 17,
    kCoapOptionLocationQuery = 20,
    kCoapOptionProxyUri = 35,
    kCoapOptionProxyScheme = 39,
    kCoapOptionSize1 = 60,
};

#define MESSAGE_CODE_FROM_CLASS_CODE(CLASS, CODE) ( (CLASS <<5) | CODE )
enum CoapMessageCode {
    None = 0,
    // 0.xx Request
    kCoapMessageCodeGet = 1,
    kCoapMessageCodePost = 2,
    kCoapMessageCodePut = 3,
    kCoapMessageCodeDelete = 4,
    // 2.xx Success
    kCoapMessageCodeCreated = MESSAGE_CODE_FROM_CLASS_CODE( 2, 01),
    kCoapMessageCodeDeleted = MESSAGE_CODE_FROM_CLASS_CODE( 2, 02),
    kCoapMessageCodeValid   = MESSAGE_CODE_FROM_CLASS_CODE( 2, 03),
    kCoapMessageCodeChanged = MESSAGE_CODE_FROM_CLASS_CODE( 2, 04),
    kCoapMessageCodeContent = MESSAGE_CODE_FROM_CLASS_CODE( 2, 05),
    // 4.xx Client Error
    kCoapMessageCodeBadRequest               = MESSAGE_CODE_FROM_CLASS_CODE( 4, 00),
    kCoapMessageCodeUnauthorized             = MESSAGE_CODE_FROM_CLASS_CODE( 4, 01),
    kCoapMessageCodeBadOption                = MESSAGE_CODE_FROM_CLASS_CODE( 4, 02),
    kCoapMessageCodeForbidden                = MESSAGE_CODE_FROM_CLASS_CODE( 4, 03),
    kCoapMessageCodeNotFound                 = MESSAGE_CODE_FROM_CLASS_CODE( 4, 04),
    kCoapMessageCodeMethodNotAllowed         = MESSAGE_CODE_FROM_CLASS_CODE( 4, 05),
    kCoapMessageCodeNotAcceptable            = MESSAGE_CODE_FROM_CLASS_CODE( 4, 06),
    kCoapMessageCodePreconditionFailed       = MESSAGE_CODE_FROM_CLASS_CODE( 4, 12),
    kCoapMessageCodeRequestEntityTooLarge    = MESSAGE_CODE_FROM_CLASS_CODE( 4, 13),
    kCoapMessageCodeUnsupportedContentFormat = MESSAGE_CODE_FROM_CLASS_CODE( 4, 15),
    // 5.xx Server Error
    kCoapMessageCodeInternalServerError  = MESSAGE_CODE_FROM_CLASS_CODE( 5, 00),
    kCoapMessageCodeNotImplemented       = MESSAGE_CODE_FROM_CLASS_CODE( 5, 01),
    kCoapMessageCodeBadGateway           = MESSAGE_CODE_FROM_CLASS_CODE( 5, 02),
    kCoapMessageCodeServiceUnavailable   = MESSAGE_CODE_FROM_CLASS_CODE( 5, 03),
    kCoapMessageCodeGatewayTimeout       = MESSAGE_CODE_FROM_CLASS_CODE( 5, 04),
    kCoapMessageCodeProxyingNotSupported = MESSAGE_CODE_FROM_CLASS_CODE( 5, 05)
};

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

typedef void *CoapMessage_t;
typedef void *CoapResource_t;

extern const int kCoapConnectedBit;

typedef CoapResult_t (*CoapResourceCallback_t)( const CoapResource_t resource, const CoapMessage_t request, CoapMessage_t response );

typedef CoapResult_t (*coap_message_get_option_uint_t)( const CoapMessage_t message, const uint16_t option, uint32_t *value );
typedef CoapResult_t (*coap_message_add_option_uint_t)( CoapMessage_t message, uint16_t option, uint32_t code );
typedef CoapResult_t (*coap_message_get_code_t)( const CoapMessage_t message, uint8_t *code );
typedef CoapResult_t (*coap_message_set_code_t)( CoapMessage_t message, uint8_t code );
typedef CoapResult_t (*coap_message_get_payload_t)( const CoapMessage_t message, uint8_t **payload, size_t *length );
typedef CoapResult_t (*coap_message_set_payload_t)( CoapMessage_t message, uint8_t *payload, size_t length );

typedef CoapResult_t (*coap_resource_create_t)( CoapResource_t *resource, const char* uri );
typedef CoapResult_t (*coap_resource_set_contnet_type_t)( CoapResource_t resource, uint16_t content_type );
typedef CoapResult_t (*coap_resource_set_callbakk_t)( CoapResource_t resource, CoapResourceCallback_t callback );

typedef CoapResult_t (*coap_unregister_reesource_t)( const CoapResource_t resource );

typedef struct{
    coap_message_get_option_uint_t   message_get_option_uint;
    coap_message_add_option_uint_t   message_add_option_uint;
    coap_message_get_code_t          message_get_code;
    coap_message_set_code_t          message_set_code;
    coap_message_get_payload_t       message_get_payload;
    coap_message_set_payload_t       message_set_payload;

    coap_resource_create_t           resource_create;
    coap_resource_set_contnet_type_t resource_set_contnet_type;
    coap_resource_set_callbakk_t     resource_set_callbakk;

    coap_unregister_reesource_t      unregister_reesource;
} CoapInterface_t;

CoapResult_t coap_init( const CoapOptions_t *options, EventGroupHandle_t wifi_events );

#endif /* __MAIN_COAP_ */