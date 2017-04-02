#include "cn-cbor/cn-cbor.h"
#include "coap.h"
#include "esp_log.h"


static const char *TAG = "OIC Handler";
static CoapInterface_t coap_interface;

const char *OIC_PLATFORM_UUID = CONFIG_IOTNODE_PLATFORM_UUID;

const char *OIC_DEVICE_UUID = "ac74515f-d789-4451-be62-075b55936ff3";

static CoapResource_t resource_oic_res;
static CoapResource_t resource_oic_p;
static CoapResource_t resource_oic_d;

static uint8_t payloadTemp[250];

CoapResult_t oic_resource_handler( const CoapResource_t resource, const CoapMessage_t request, CoapMessage_t response )
{
    cn_cbor *result, *links;
    size_t enc_sz;
    uint8_t request_code = 0;
    coap_interface.message_get_code( request, &request_code );

    // Only allow GET requests
    if( request_code != kCoapMessageCodeGet )
    {
        coap_interface.message_set_code( response, kCoapMessageCodeMethodNotAllowed );
        return kCoapOK;
    }

    result = cn_cbor_map_create( NULL );
    
    if( resource == resource_oic_res )
    {
        cn_cbor_map_put( result,
            cn_cbor_string_create( "n", NULL ),
            cn_cbor_string_create( CONFIG_IOTNODE_HOSTNAME, NULL ),
            NULL
        );

        cn_cbor_map_put( result,
            cn_cbor_string_create( "di", NULL ),
            cn_cbor_string_create( OIC_DEVICE_UUID, NULL ),
            NULL
        );

        links = cn_cbor_array_create( NULL );

        cn_cbor_map_put( result, cn_cbor_string_create( "links", NULL ), links, NULL );
    } 
    else if( resource == resource_oic_p )
    {
        cn_cbor_map_put( result,
            cn_cbor_string_create( "pi", NULL ),
            cn_cbor_string_create( OIC_PLATFORM_UUID, NULL ),
            NULL
        );

        cn_cbor_map_put( result,
            cn_cbor_string_create( "mnmn", NULL ),
            cn_cbor_string_create( CONFIG_IOTNODE_MANUFACTURER_NAME, NULL ),
            NULL
        );

        if( CONFIG_IOTNODE_MANUFACTURER_URL[0] != 0 )
        {
            cn_cbor_map_put( result,
                cn_cbor_string_create( "mnml", NULL ),
                cn_cbor_string_create( CONFIG_IOTNODE_MANUFACTURER_URL, NULL ),
                NULL
            );
        }
        
        if( CONFIG_IOTNODE_MODEL[0] != 0 )
        {
            cn_cbor_map_put( result,
                cn_cbor_string_create( "mnmo", NULL ),
                cn_cbor_string_create( CONFIG_IOTNODE_MODEL, NULL ),
                NULL
            );
        }
    } 
    else if( resource == resource_oic_d )
    {
         cn_cbor_map_put( result,
            cn_cbor_string_create( "n", NULL ),
            cn_cbor_string_create( CONFIG_IOTNODE_HOSTNAME, NULL ),
            NULL
        );

        cn_cbor_map_put( result,
            cn_cbor_string_create( "di", NULL ),
            cn_cbor_string_create( OIC_DEVICE_UUID, NULL ),
            NULL
        );

        cn_cbor_map_put( result,
            cn_cbor_string_create( "icv", NULL ),
            cn_cbor_string_create( "core.1.1.0", NULL ),
            NULL
        );
        cn_cbor_map_put( result,
            cn_cbor_string_create( "dmv", NULL ),
            cn_cbor_string_create( "res.1.1.0", NULL ),
            NULL
        );
    } 
    else 
    {
        coap_interface.message_set_code( response, kCoapMessageCodeNotFound );

        cn_cbor_free( result );
        return kCoapOK;
    }

    enc_sz = cn_cbor_encoder_write( payloadTemp, 0, 250, result );

    coap_interface.message_set_code( response, kCoapMessageCodeContent );
    coap_interface.message_add_option_uint( response, kCoapOptionContentFormat, kCoapContentTypeApplicationCbor );
    coap_interface.message_set_payload( response, payloadTemp, enc_sz );
    
    cn_cbor_free( result );
    return kCoapOK;
}

void oic_init ( const CoapInterface_t interface )
{
    coap_interface = interface;
    // Todo: Register mandatory /oic/res, /oic/p, /oic/d
    if( coap_interface.resource_create( &resource_oic_res, "/oic/res" ) == kCoapOK )
    {
        coap_interface.resource_set_contnet_type( resource_oic_res, kCoapContentTypeApplicationJson );
        coap_interface.resource_set_callbakk( resource_oic_res, oic_resource_handler );
    }
    if( coap_interface.resource_create( &resource_oic_p, "/oic/p" ) == kCoapOK )
    {
        coap_interface.resource_set_contnet_type( resource_oic_p, kCoapContentTypeApplicationJson );
        coap_interface.resource_set_callbakk( resource_oic_p, oic_resource_handler );
    }

    if( coap_interface.resource_create( &resource_oic_d, "/oic/d" ) == kCoapOK )
    {
        coap_interface.resource_set_contnet_type( resource_oic_d, kCoapContentTypeApplicationJson );
        coap_interface.resource_set_callbakk( resource_oic_d, oic_resource_handler );
    }

    // Todo: register optional /oic/con, /oic/mnt
}

void oic_register_resource( const char *base_uri )
{

}