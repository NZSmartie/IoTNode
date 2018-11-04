#ifndef _RESOURCES_SWITCH_H_
#define _RESOURCES_SWITCH_H_

#include "driver/gpio.h"

#include "coap.h"

class SwitchResource : public IApplicationResource {
public:
    enum class State
    {
        Idle,
        Pushed,
    };
private:
    ICoapInterface& _coap;
    CoapResource _resource;
    xTaskHandle _task;
    xSemaphoreHandle _switchSemaphore;

    static void TaskHandle(void* pvParameters);

    const gpio_num_t _pin;
    const uint32_t _activeLevel;

    State _state = State::Idle;

    static void GPIOHandler(void* arg);
    void Respond(ICoapMessage *response, CoapContentType contentType, CoapResult &result);
public:
    SwitchResource(ICoapInterface& coap, gpio_num_t pin, uint32_t activeLevel = 0);
    void HandleRequest(ICoapMessage const *request, ICoapMessage *response, CoapResult &result);
    void HandleNotify(ICoapObserver const *observer, ICoapMessage *response, CoapResult &result);

    State GetSate() const { return _state; }
};

#endif /* _RESOURCES_SWITCH_H_ */
