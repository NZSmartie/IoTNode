#include <string.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/xtensa_api.h"
#include "freertos/queue.h"
#include "driver/ledc.h"
#include "esp_attr.h"   
#include "esp_err.h"

#include "../coap.h"

#include "esp_log.h"

#include "parson.h"

CoapResource_t led_resource = NULL;
static const char *TAG = "LEDs Resource";

static double red, green, blue;

#define LEDC_IO_0    (33)
#define LEDC_IO_1    (32)
#define LEDC_IO_2    (26)

static CoapInterface_t _coap;

// LED PWM Lookup table
static int LED_LOOKUP[] = {
	0, 21, 22, 23, 24, 26, 27, 28, 30, 31, 33, 34, 36, 38, 39, 41,
    43, 45, 48, 50, 52, 55, 57, 60, 63, 66, 69, 73, 76, 80, 84, 88,
    92, 97, 101, 106, 111, 117, 122, 128, 134, 141, 147, 154, 162, 170, 178, 186,
    195, 204, 214, 224, 235, 246, 257, 269, 282, 295, 309, 324, 339, 355, 371, 389,
    407, 425, 445, 465, 487, 509, 532, 556, 582, 608, 635, 664, 693, 724, 756, 790,
    824, 860, 898, 936, 977, 1018, 1061, 1106, 1153, 1201, 1250, 1301, 1354, 1409, 1466, 1524,
    1584, 1645, 1709, 1774, 1841, 1910, 1981, 2053, 2127, 2203, 2281, 2360, 2441, 2523, 2607, 2692,
    2779, 2867, 2957, 3047, 3139, 3231, 3325, 3420, 3515, 3611, 3707, 3804, 3901, 3998, 4096, 4194,
    4291, 4388, 4485, 4581, 4677, 4772, 4867, 4961, 5053, 5145, 5235, 5325, 5413, 5500, 5585, 5669,
    5751, 5832, 5911, 5989, 6065, 6139, 6211, 6282, 6351, 6418, 6483, 6547, 6608, 6668, 6726, 6783,
    6838, 6891, 6942, 6991, 7039, 7086, 7131, 7174, 7215, 7256, 7294, 7332, 7368, 7402, 7436, 7468,
    7499, 7528, 7557, 7584, 7610, 7636, 7660, 7683, 7705, 7727, 7747, 7767, 7785, 7803, 7821, 7837,
    7853, 7868, 7883, 7897, 7910, 7923, 7935, 7946, 7957, 7968, 7978, 7988, 7997, 8006, 8014, 8022,
    8030, 8038, 8045, 8051, 8058, 8064, 8070, 8075, 8081, 8086, 8091, 8095, 8100, 8104, 8108, 8112,
    8116, 8119, 8123, 8126, 8129, 8132, 8135, 8137, 8140, 8142, 8144, 8147, 8149, 8151, 8153, 8154,
    8156, 8158, 8159, 8161, 8162, 8164, 8165, 8166, 8168, 8169, 8170, 8171, 8172, 8173, 8174, 8174
};

static CoapResult_t led_requesthandler( const CoapResource_t resource, const CoapMessage_t request, CoapMessage_t response )
{
	char payloadTemp[250];

    JSON_Value *root_value;
    JSON_Object *root_object;
    JSON_Array *json_leds_color;

    uint32_t accept = 0;
    if( _coap.message_get_option_uint( request, kCoapOptionAccept, &accept ) != kCoapOK )
        accept = kCoapContentTypeApplicationJson;

    if( accept != kCoapContentTypeApplicationJson )
    {
        ESP_LOGE( TAG, "message_get_option_uint: Accept Option (%d) != %d", accept, kCoapMessageCodeBadOption )
        _coap.message_set_code( response, kCoapMessageCodeBadOption );
        return kCoapError;
    }

    _coap.message_set_code(response, kCoapMessageCodeContent );
    
    uint8_t requestCode;
    _coap.message_get_code(request, &requestCode );
    
    if( requestCode != kCoapMessageCodeGet && requestCode != kCoapMessageCodePut ) 
    {
        _coap.message_set_code( response, kCoapMessageCodeNotAcceptable );
        return kCoapError;
    }
    if( requestCode == kCoapMessageCodePut ) 
    {
        uint8_t *payload;
        _coap.message_get_payload( request, &payload, NULL );
        root_value = json_parse_string( (char*) payload );
        if( root_value == NULL )
        {
            _coap.message_set_code( response, kCoapMessageCodeBadRequest );
            return kCoapError;
        }
        root_object = json_value_get_object( root_value );
        if( root_object == NULL )
        {
            json_value_free( root_value );
            _coap.message_set_code( response, kCoapMessageCodeBadRequest );
            return kCoapError;
        }

        json_leds_color = json_object_dotget_array( root_object , "leds.colour" );
        if( json_leds_color == NULL )
        {
            json_value_free( root_value );
            _coap.message_set_code( response, kCoapMessageCodeBadRequest );
            return kCoapError;
        }

        red = json_array_get_number( json_leds_color, 0 );
        if (red > 255 ) red = 255;
        if (red < 0 ) red = 0;
        green = json_array_get_number( json_leds_color, 1 );
        if (green > 255 ) green = 255;
        if (green < 0 ) green = 0;
        blue = json_array_get_number( json_leds_color, 2 );
        if (blue > 255 ) blue = 255;
        if (blue < 0 ) blue = 0;

        ESP_LOGD( TAG, "PUT Request with RGB Values: [%f, %f, %f]", red, green, blue );

        json_value_free( root_value );
        _coap.message_set_code(response, kCoapMessageCodeChanged );

        ledc_set_fade_with_time( LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, LED_LOOKUP[ lrint( red ) ], 1000 );
        ledc_set_fade_with_time( LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, LED_LOOKUP[ lrint( green ) ], 1000 );
        ledc_set_fade_with_time( LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_2, LED_LOOKUP[ lrint( blue ) ], 1000 );
        ledc_fade_start( LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, LEDC_FADE_NO_WAIT );
        ledc_fade_start( LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, LEDC_FADE_NO_WAIT );
        ledc_fade_start( LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_2, LEDC_FADE_NO_WAIT );
    }

    root_value = json_value_init_object();
    root_object = json_value_get_object(root_value);
    json_leds_color = json_array( json_value_init_array() );

    json_array_append_number( json_leds_color, red );
    json_array_append_number( json_leds_color, green );
    json_array_append_number( json_leds_color, blue );
    
    json_object_dotset_value( root_object, "leds.colour", json_array_get_wrapping_value( json_leds_color ) );

    json_serialize_to_buffer( root_value, payloadTemp, 250 );
    json_value_free(root_value);

    _coap.message_add_option_uint( response, kCoapOptionContentFormat, kCoapContentTypeApplicationJson );
    _coap.message_set_payload( response, (uint8_t*)payloadTemp, strlen( payloadTemp ) );

    return kCoapOK;
}

void coap_create_led_resource( CoapInterface_t coap )
{
    _coap = coap;
    
    ledc_timer_config_t ledc_timer = {
        //set timer counter bit number
        .bit_num = LEDC_TIMER_13_BIT,
        //set frequency of pwm
        .freq_hz = 5000,
        //timer mode,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        //timer index
        .timer_num = LEDC_TIMER_0
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        //set LEDC channel 0
        .channel = LEDC_CHANNEL_0,
        //set the duty for initialization.(duty range is 0 ~ ((2**bit_num)-1)
        .duty = 0,
        //GPIO number
        .gpio_num = LEDC_IO_0,
        //GPIO INTR TYPE, as an example, we enable fade_end interrupt here.
        .intr_type = LEDC_INTR_FADE_END,
        //set LEDC mode, from ledc_mode_t
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        //set LEDC timer source, if different channel use one timer,
        //the frequency and bit_num of these channels should be the same
        .timer_sel = LEDC_TIMER_0
    };
    //set the configuration
    ledc_channel_config(&ledc_channel);

    //config ledc channel1
    ledc_channel.channel = LEDC_CHANNEL_1;
    ledc_channel.gpio_num = LEDC_IO_1;
    ledc_channel_config(&ledc_channel);
    //config ledc channel2
    ledc_channel.channel = LEDC_CHANNEL_2;
    ledc_channel.gpio_num = LEDC_IO_2;
    ledc_channel_config(&ledc_channel);

    //initialize fade service.
    ledc_fade_func_install(0);

    ledc_set_fade_with_time( LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 0, 1000 );
    ledc_set_fade_with_time( LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, 0, 1000 );
    ledc_set_fade_with_time( LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_2, 0, 1000 );
    ledc_fade_start( LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, LEDC_FADE_NO_WAIT );
    ledc_fade_start( LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, LEDC_FADE_NO_WAIT );
    ledc_fade_start( LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_2, LEDC_FADE_NO_WAIT );

    if( coap.resource_create( &led_resource, "led" ) != kCoapOK ){
        ESP_LOGE( TAG, "coap_resource_create failed" );
        return;
    }

    if( coap.resource_set_contnet_type( led_resource, kCoapContentTypeApplicationJson ) != kCoapOK ){
        ESP_LOGE( TAG, "coap_resource_set_contnet_type failed" );
        return;
    }
    if( coap.resource_set_callbakk( led_resource, led_requesthandler ) != kCoapOK ){
        ESP_LOGE( TAG, "coap_resource_set_callbakk failed" );
        return;
    }
}