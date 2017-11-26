#include "cn-cbor/cn-cbor.h"
#include "coap.h"
#include "oic.h"

#ifdef LOG_LOCAL_LEVEL
    #undef LOG_LOCAL_LEVEL
#endif 

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include "esp_log.h"


static const char *TAG = "OIC Handler";
static CoapInterface_t coap_interface;

const char *OIC_PLATFORM_UUID = CONFIG_IOTNODE_PLATFORM_UUID;

const char *OIC_DEVICE_UUID = "ac74515f-d789-4451-be62-075b55936ff3";

static CoapResource_t resource_oic_res;
static CoapResource_t resource_oic_p;
static CoapResource_t resource_oic_d;

static uint8_t payloadTemp[250];

typedef struct OICResourceList_s
{
    const OICResource_t* resource;
    CoapResource_t coap_resource;
    struct OICResourceList_s *next;
} OICResourceList_t;

static OICResourceList_t *resource_list = NULL;

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
    
    if( resource == resource_oic_res )
    {
        result = cn_cbor_array_create( NULL );

        cn_cbor *map = cn_cbor_map_create( NULL );

        cn_cbor_map_put( map,
            cn_cbor_string_create( "n", NULL ),
            cn_cbor_string_create( CONFIG_IOTNODE_HOSTNAME, NULL ),
            NULL
        );

        {
            cn_cbor *rt = cn_cbor_array_create( NULL );
            cn_cbor_array_append( rt, cn_cbor_string_create( "oic.wk.res", NULL ), NULL );
            cn_cbor_map_put( map, cn_cbor_string_create( "rt", NULL ), rt, NULL );
        }

        cn_cbor_map_put( map,
            cn_cbor_string_create( "di", NULL ),
            cn_cbor_string_create( OIC_DEVICE_UUID, NULL ),
            NULL
        );

        links = cn_cbor_array_create( NULL );

        OICResourceList_t *resource = resource_list;
        while( resource != NULL ){
            cn_cbor *link = cn_cbor_map_create( NULL ), *linkItems;

            cn_cbor_map_put( link,
                cn_cbor_string_create( "href", NULL ),
                cn_cbor_string_create( resource->resource->href, NULL ),
                NULL
            );

            linkItems = cn_cbor_array_create( NULL );
            if( ( resource->resource->interfaces & kOICInterfaceBaseline ) != 0)
                cn_cbor_array_append( linkItems, cn_cbor_string_create( "oic.if.baseline", NULL ), NULL );
            if( ( resource->resource->interfaces & kOICInterfaceLinkLists ) != 0)
                cn_cbor_array_append( linkItems, cn_cbor_string_create( "oic.if.ll", NULL ), NULL );
            if( ( resource->resource->interfaces & kOICInterfaceBatch ) != 0)
                cn_cbor_array_append( linkItems, cn_cbor_string_create( "oic.if.b", NULL ), NULL );
            if( ( resource->resource->interfaces & kOICInterfaceReadWrite ) != 0)
                cn_cbor_array_append( linkItems, cn_cbor_string_create( "oic.if.rw", NULL ), NULL );
            if( ( resource->resource->interfaces & kOICInterfaceReadOnly ) != 0)
                cn_cbor_array_append( linkItems, cn_cbor_string_create( "oic.if.r", NULL ), NULL );
            if( ( resource->resource->interfaces & kOICInterfaceActuator ) != 0)
                cn_cbor_array_append( linkItems, cn_cbor_string_create( "oic.if.a", NULL ), NULL );
            if( ( resource->resource->interfaces & kOICInterfaceSensor ) != 0)
                cn_cbor_array_append( linkItems, cn_cbor_string_create( "oic.if.s", NULL ), NULL );

            cn_cbor_map_put( link, cn_cbor_string_create( "if", NULL ), linkItems, NULL );

            linkItems = cn_cbor_array_create( NULL );
            for( int i = 0; i < resource->resource->resource_types_count; i++)
                cn_cbor_array_append( linkItems, cn_cbor_string_create( resource->resource->resource_types[i], NULL ), NULL );
            cn_cbor_map_put( link, cn_cbor_string_create( "rt", NULL ), linkItems, NULL );

            if(resource->resource->name != NULL && resource->resource->name[0] != '\0'){
                cn_cbor_map_put( link,
                    cn_cbor_string_create( "title", NULL ),
                    cn_cbor_string_create( resource->resource->name, NULL ),
                    NULL
                );
            }

            cn_cbor_array_append( links, link, NULL );
            resource = resource->next;
        }

        cn_cbor_map_put( map, cn_cbor_string_create( "links", NULL ), links, NULL );

        cn_cbor_array_append( result, map , NULL );
    } 
    else if( resource == resource_oic_p )
    {
        result = cn_cbor_map_create( NULL );

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
        result = cn_cbor_map_create( NULL );

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
        coap_interface.resource_set_contnet_type( resource_oic_res, kCoapContentTypeApplicationCbor );
        coap_interface.resource_set_callbakk( resource_oic_res, oic_resource_handler );
    }
    if( coap_interface.resource_create( &resource_oic_p, "/oic/p" ) == kCoapOK )
    {
        coap_interface.resource_set_contnet_type( resource_oic_p, kCoapContentTypeApplicationCbor );
        coap_interface.resource_set_callbakk( resource_oic_p, oic_resource_handler );
    }

    if( coap_interface.resource_create( &resource_oic_d, "/oic/d" ) == kCoapOK )
    {
        coap_interface.resource_set_contnet_type( resource_oic_d, kCoapContentTypeApplicationCbor );
        coap_interface.resource_set_callbakk( resource_oic_d, oic_resource_handler );
    }

    // Todo: register optional /oic/con, /oic/mnt
}

void oic_register_resource( const OICResource_t *resource )
{
    OICResourceList_t *resource_item = resource_list;

    ESP_LOGD( TAG, "Registering OIC resource %s", resource->href );

    if( resource_list == NULL ){
        resource_item = resource_list = malloc( sizeof( OICResourceList_t ) );
        ESP_LOGD( TAG, "Creating new resource list" );
    } 
    else 
    {
        while( resource_item->next != NULL )
            resource_item = resource_item->next;
        resource_item->next = malloc( sizeof( OICResourceList_t ) );
        resource_item = resource_item->next;
        ESP_LOGD( TAG, "Added new resource to the list" );
    }

    resource_item->resource = resource;
    resource_item->next = NULL;

    if( coap_interface.resource_create( &resource_item->coap_resource, resource->href ) != kCoapOK ){
        ESP_LOGE( TAG, "coap_resource_create failed" );
        return;
    }

    if( coap_interface.resource_set_contnet_type( resource_item->coap_resource, kCoapContentTypeApplicationJson ) != kCoapOK ){
        ESP_LOGE( TAG, "coap_resource_set_contnet_type failed" );
        return;
    }
    
    if( coap_interface.resource_set_callbakk( resource_item->coap_resource, resource_item->resource->callback ) != kCoapOK ){
        ESP_LOGE( TAG, "coap_resource_set_callbakk failed" );
        return;
    }
}