#include <nlohmann/json.hpp>

#include <cstring>
#include <ostream>
#include <sstream>
#include <string>

// for convenience
using json = nlohmann::json;

#include "driver/gpio.h"
#include "esp_log.h"
#include "tcpip_adapter.h"

#include "resources/switch.h"

static const char *kTag = "Switch Resource";

static const char *kThreadName = "Switch Resource";
static const size_t kThreadStackSize = 2048;
static const UBaseType_t kThreadPriority = 8;
static constexpr int kDebouncePeriod = 20;

void IRAM_ATTR SwitchResource::GPIOHandler(void* arg)
{
    auto instance = static_cast<SwitchResource*>(arg);
    BaseType_t sHigherPriorityTaskWoken;
	xSemaphoreGiveFromISR(instance->_switchSemaphore, &sHigherPriorityTaskWoken);
}

void SwitchResource::Respond(ICoapMessage *response, CoapContentType contentType, CoapResult &result)
{
    json output;
    output["state"] = _state == State::Idle ? "idle" : "pushed";

    CoapMessageCode code = CoapMessageCode::Content;

    if(contentType == CoapContentType::ApplicationJson)
    {
        response->SetOption(CoapUIntOption(CoapOptionValue::ContentFormat, CoapContentType::ApplicationJson), result);
        response->SetPayload(output.dump(), result);
    }
    else if(contentType == CoapContentType::ApplicationCbor)
    {
        response->SetOption(CoapUIntOption(CoapOptionValue::ContentFormat, CoapContentType::ApplicationCbor), result);
        response->SetPayload(json::to_cbor(output), result);
    }
    else
    {
        code = CoapMessageCode::BadOption;
        result = CoapResult::Error;
    }

    response->SetCode(code, result);
}

void SwitchResource::HandleNotify(ICoapObserver const *observer, ICoapMessage *response, CoapResult &result)
{
    CoapOption acceptOption;
    observer->GetOption(acceptOption, CoapOptionValue::Accept, result);

    // Default to text/plain if the option wasn't present
    CoapContentType accept = result == CoapResult::OK
        ? static_cast<CoapContentType>(AsUInt(acceptOption)->Value)
        : CoapContentType::ApplicationJson;

    Respond(response, accept, result);
}

void SwitchResource::HandleRequest(ICoapMessage const *request, ICoapMessage *response, CoapResult &result)
{
    CoapOption acceptOption;
    request->GetOption(acceptOption, CoapOptionValue::Accept, result);

    // Default to text/plain if the option wasn't present
    CoapContentType accept = result == CoapResult::OK
        ? static_cast<CoapContentType>(AsUInt(acceptOption)->Value)
        : CoapContentType::ApplicationJson;

    Respond(response, accept, result);
}

SwitchResource::SwitchResource(ICoapInterface& coap, gpio_num_t pin, uint32_t activeLevel)
    : _coap(coap), _pin(pin), _activeLevel(activeLevel)
{
    CoapResult result;
    this->_coap.CreateResource(this->_resource, this, "switch", result);
    if (result != CoapResult::OK || this->_resource == nullptr)
    {
        ESP_LOGE( kTag, "CreateResource failed." );
        return;
    }

    this->_resource->RegisterHandler(CoapMessageCode::Get, result);
    this->_resource->RegisterAsObservable(result);

    int ret = xTaskCreate(
        &SwitchResource::TaskHandle,
        kThreadName,
        kThreadStackSize,
        this,
        kThreadPriority,
        &this->_task
    );

    if (ret != true)
        ESP_LOGE(kTag, "Failed to create thread %s", kThreadName );

    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    //set as output mode
    io_conf.mode = GPIO_MODE_INPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = (1 << _pin);
    //disable pull-down mode
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    //disable pull-up mode
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    //install gpio isr service
    gpio_install_isr_service(0);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(_pin, &SwitchResource::GPIOHandler, this);
}

void SwitchResource::TaskHandle(void* pvParameters)
{
    auto instance = static_cast<SwitchResource*>(pvParameters);

    vSemaphoreCreateBinary(instance->_switchSemaphore);

    instance->_state = gpio_get_level(instance->_pin) == instance->_activeLevel
            ? State::Pushed
            : State::Idle;
    while(true)
    {
        xSemaphoreTake(instance->_switchSemaphore, portMAX_DELAY);

        State state = gpio_get_level(instance->_pin) == instance->_activeLevel
            ? State::Pushed
            : State::Idle;


        if(state != instance->_state)
        {
            CoapResult result;
            instance->_state = state;
            instance->_resource->NotifyObservers(result);
        }

        vTaskDelay(kDebouncePeriod / portTICK_PERIOD_MS);
    }
}
