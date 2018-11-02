#ifndef _RESOURCES_LED_H_
#define _RESOURCES_LED_H_

#include "driver/ledc.h"

#include "coap.h"

class LEDResource : public IApplicationResource {
public:
    enum class Mode
    {
        ShowStatus,
        User
    };
private:
    ICoapInterface& _coap;
    CoapResource _resource;

    const gpio_num_t _pinLEDRed;
    const gpio_num_t _pinLEDGreen;
    const gpio_num_t _pinLEDBlue;

    uint8_t _redValue = 0;
    uint8_t _greenValue = 0;
    uint8_t _blueValue = 0;

    Mode _mode = Mode::ShowStatus;

    ledc_channel_config_t _ledcRedChannel;
    ledc_channel_config_t _ledcGreenChannel;
    ledc_channel_config_t _ledcBlueChannel;
public:
    LEDResource(ICoapInterface& coap, gpio_num_t red, gpio_num_t green, gpio_num_t blue);
    void HandleRequest(ICoapMessage const *request, ICoapMessage *response, CoapResult &result);

    void SetStatusColor(uint8_t red, uint8_t green, uint8_t blue, int fadeTime = 0);

    void SetMode(Mode mode);
    Mode GetMode() const { return _mode; }

    void SetColor(uint8_t red, uint8_t green, uint8_t blue);
    void GetColor(uint8_t &red, uint8_t &green, uint8_t &blue);
};

#endif /* _RESOURCES_LED_H_ */
