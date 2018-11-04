#include <nlohmann/json.hpp>

#include <cstring>
#include <ostream>
#include <sstream>
#include <string>

// for convenience
using json = nlohmann::json;

#include "driver/ledc.h"
#include "esp_log.h"
#include "tcpip_adapter.h"

#include "utils.h"
#include "resources/led.h"

static const char *kTag = "LED Resource";

static const ledc_timer_t   kLedcTimer = LEDC_TIMER_0;
static const ledc_mode_t    kLedcMode = LEDC_HIGH_SPEED_MODE;
static const ledc_channel_t kLedcCh0Channel = LEDC_CHANNEL_0;
static const ledc_channel_t kLedcCh1Channel = LEDC_CHANNEL_1;
static const ledc_channel_t kLedcCh2Channel = LEDC_CHANNEL_2;

static const ledc_timer_bit_t kLedcResolution = LEDC_TIMER_10_BIT;
static constexpr int kLedcHPoint = (1 << kLedcResolution) - 1;

static const int kFadeTime = 200;

void LEDResource::HandleRequest(ICoapMessage const *request, ICoapMessage *response, CoapResult &result)
{
    CoapOption acceptOption;
    request->GetOption(acceptOption, CoapOptionValue::Accept, result);
    uint8_t red, green, blue;

    // Default to text/plain if the option wasn't present
    uint32_t accept = result == CoapResult::OK
        ? static_cast<CoapContentType>(AsUInt(acceptOption)->Value)
        : CoapContentType::ApplicationJson;

    CoapMessageCode code = CoapMessageCode::Content;

    if(request->GetCode() == CoapMessageCode::Post)
    {
        CoapOption contentOption;
        request->GetOption(contentOption, CoapOptionValue::ContentFormat, result);

        if(result == CoapResult::OK)
        {
            Payload inputPayload;
            request->GetPayload(inputPayload, result);
            if(result != CoapResult::OK)
            {
                response->SetCode(CoapMessageCode::BadRequest, result);
                result = CoapResult::Error;
                return;
            }

            auto input = json::parse(inputPayload);
            auto color = input.find("color");
            auto mode = input.find("mode");
            if(color != input.end() && (*color).is_array() && (*color).size() == 3)
            {
                red = (*color)[0].get<uint8_t>();
                green = (*color)[1].get<uint8_t>();
                blue = (*color)[2].get<uint8_t>();
                SetColor(red, green, blue);
            }
            if(mode != input.end() && (*mode).is_string())
            {
                if(*mode == "status")
                    SetMode(Mode::ShowStatus);
                else if(*mode == "user")
                    SetMode(Mode::User);
            }

            code = CoapMessageCode::Changed;
        }
    }

    json output;
    GetColor(red, green, blue);
    output["color"] = { red, green, blue };
    output["mode"] = _mode == Mode::ShowStatus ? "status" : "user";

    if(accept == CoapContentType::ApplicationJson)
    {
        response->AddOption(CoapUIntOption(CoapOptionValue::ContentFormat, CoapContentType::ApplicationJson), result);
        response->SetPayload(output.dump(), result);
    }
    else if(accept == CoapContentType::ApplicationCbor)
    {
        response->AddOption(CoapUIntOption(CoapOptionValue::ContentFormat, CoapContentType::ApplicationCbor), result);
        response->SetPayload(json::to_cbor(output), result);
    }
    else
    {
        code = CoapMessageCode::BadOption;
        result = CoapResult::Error;
    }

    response->SetCode(code, result);
}

LEDResource::LEDResource(ICoapInterface& coap, gpio_num_t red, gpio_num_t green, gpio_num_t blue)
    : _coap(coap), _pinLEDRed(red), _pinLEDGreen(green), _pinLEDBlue(blue)
{
    CoapResult result;
    this->_coap.CreateResource(this->_resource, this, "led", result);
    if (result != CoapResult::OK || this->_resource == nullptr)
    {
        ESP_LOGE( kTag, "CreateResource failed." );
        return;
    }

    this->_resource->RegisterHandler(CoapMessageCode::Get, result);
    this->_resource->RegisterHandler(CoapMessageCode::Post, result);

        /*
     * Prepare and set configuration of timers
     * that will be used by LED Controller
     */
    ledc_timer_config_t ledc_timer;
        ledc_timer.duty_resolution = kLedcResolution; // resolution of PWM duty
        ledc_timer.freq_hz = 5000;                      // frequency of PWM signal
        ledc_timer.speed_mode = LEDC_HIGH_SPEED_MODE;           // timer mode
        ledc_timer.timer_num = LEDC_TIMER_0;           // timer index

    // Set configuration of timer0 for high speed channels
    ledc_timer_config(&ledc_timer);


    _ledcRedChannel.channel    = kLedcCh0Channel;
    _ledcRedChannel.duty       = 0;
    _ledcRedChannel.gpio_num   = _pinLEDRed;
    _ledcRedChannel.speed_mode = kLedcMode;
    _ledcRedChannel.timer_sel  = LEDC_TIMER_0;
    _ledcRedChannel.hpoint     = kLedcHPoint;

    _ledcGreenChannel.channel    = kLedcCh1Channel;
    _ledcGreenChannel.duty       = 0;
    _ledcGreenChannel.gpio_num   = _pinLEDGreen;
    _ledcGreenChannel.speed_mode = kLedcMode;
    _ledcGreenChannel.timer_sel  = LEDC_TIMER_0;
    _ledcGreenChannel.hpoint     = kLedcHPoint;

    _ledcBlueChannel.channel    = kLedcCh2Channel;
    _ledcBlueChannel.duty       = 0;
    _ledcBlueChannel.gpio_num   = _pinLEDBlue;
    _ledcBlueChannel.speed_mode = kLedcMode;
    _ledcBlueChannel.timer_sel  = LEDC_TIMER_0;
    _ledcBlueChannel.hpoint     = kLedcHPoint;


    // Set LED Controller with previously prepared configuration
    ledc_channel_config(&_ledcRedChannel);
    ledc_channel_config(&_ledcGreenChannel);
    ledc_channel_config(&_ledcBlueChannel);

    // Initialize fade service.
    ledc_fade_func_install(0);

}

void LEDResource::SetStatusColor(uint8_t red, uint8_t green, uint8_t blue, int fadeTime)
{
    if(_mode != Mode::ShowStatus)
        return;

    ledc_set_fade_time_and_start(_ledcRedChannel.speed_mode, _ledcRedChannel.channel, red << 2, fadeTime, LEDC_FADE_NO_WAIT);
    ledc_set_fade_time_and_start(_ledcGreenChannel.speed_mode, _ledcGreenChannel.channel, green << 2, fadeTime, LEDC_FADE_NO_WAIT);
    ledc_set_fade_time_and_start(_ledcBlueChannel.speed_mode, _ledcBlueChannel.channel, blue << 2, fadeTime, LEDC_FADE_NO_WAIT);
}

void LEDResource::SetMode(Mode mode)
{
    if(_mode == mode)
        return;

    _mode = mode;

    if(_mode == Mode::ShowStatus)
    {
        ledc_set_fade_time_and_start(_ledcRedChannel.speed_mode, _ledcRedChannel.channel, 0, kFadeTime, LEDC_FADE_NO_WAIT);
        ledc_set_fade_time_and_start(_ledcGreenChannel.speed_mode, _ledcGreenChannel.channel, 0, kFadeTime, LEDC_FADE_NO_WAIT);
        ledc_set_fade_time_and_start(_ledcBlueChannel.speed_mode, _ledcBlueChannel.channel, 0, kFadeTime, LEDC_FADE_NO_WAIT);
    }
    else if(mode == Mode::User)
    {
        ledc_set_fade_time_and_start(_ledcRedChannel.speed_mode, _ledcRedChannel.channel, _redValue, kFadeTime, LEDC_FADE_NO_WAIT);
        ledc_set_fade_time_and_start(_ledcGreenChannel.speed_mode, _ledcGreenChannel.channel, _greenValue, kFadeTime, LEDC_FADE_NO_WAIT);
        ledc_set_fade_time_and_start(_ledcBlueChannel.speed_mode, _ledcBlueChannel.channel, _blueValue, kFadeTime, LEDC_FADE_NO_WAIT);
    }
}

void LEDResource::SetColor(uint8_t red, uint8_t green, uint8_t blue)
{
    _redValue = red << 2;
    _greenValue = green << 2;
    _blueValue = blue << 2;

    if(_mode != Mode::User)
        return;

    ledc_set_fade_time_and_start(_ledcRedChannel.speed_mode, _ledcRedChannel.channel, _redValue, kFadeTime, LEDC_FADE_NO_WAIT);
    ledc_set_fade_time_and_start(_ledcGreenChannel.speed_mode, _ledcGreenChannel.channel, _greenValue, kFadeTime, LEDC_FADE_NO_WAIT);
    ledc_set_fade_time_and_start(_ledcBlueChannel.speed_mode, _ledcBlueChannel.channel, _blueValue, kFadeTime, LEDC_FADE_NO_WAIT);
}

void LEDResource::GetColor(uint8_t &red, uint8_t &green, uint8_t &blue)
{
    red = _redValue >> 2;
    green = _greenValue >> 2;
    blue = _blueValue >> 2;
}
