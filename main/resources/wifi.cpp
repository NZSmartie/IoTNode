#include <nlohmann/json.hpp>

#include <cstring>
#include <ostream>
#include <sstream>
#include <string>

// for convenience
using json = nlohmann::json;

#include "esp_log.h"
#include "tcpip_adapter.h"

#include "utils.h"
#include "resources/wifi.h"

static const char *kTag = "Wifi Resource";


void WifiResource::HandleRequest(ICoapMessage const *request, ICoapMessage *response, CoapResult &result)
{
	tcpip_adapter_ip_info_t ipinfo;
	// char payloadTemp[250], ip_address[16], ip_mask[16], ip_gateway[16];
	// char* pStrWorking = payloadTemp;

    CoapOption acceptOption;
    request->GetOption(acceptOption, CoapOptionValue::Accept, result);

    // Default to text/plain if the option wasn't present
    uint32_t accept = result == CoapResult::OK
        ? static_cast<CoapContentType>(AsUInt(acceptOption)->Value)
        : CoapContentType::TextPlain;

	if( tcpip_adapter_get_ip_info( TCPIP_ADAPTER_IF_STA, &ipinfo ) != ESP_OK )
    {
        ESP_LOGE( kTag, "tcpip_adapter_get_ip_info: failed" );
        response->SetCode(CoapMessageCode::InternalServerError, result);
        response->SetPayload("failed to get ip address information", result);
        result = CoapResult::OK;
        return;
    }

    if(accept == CoapContentType::TextPlain)
    {
        std::ostringstream output;
        output << "IP: " << ipinfo.ip << ", Mask: " << ipinfo.netmask << ", Gateway: " << ipinfo.gw;

        response->AddOption(CoapUIntOption(CoapOptionValue::ContentFormat, CoapContentType::TextPlain), result);
        response->SetPayload(output.str(), result);
        response->SetCode(CoapMessageCode::Content, result);
    }
    else if(accept == CoapContentType::ApplicationJson)
    {
        json output;
        output["ip"] = to_string(ipinfo.ip);
        output["mask"] = to_string(ipinfo.netmask);
        output["gateway"] = to_string(ipinfo.gw);

        response->AddOption(CoapUIntOption(CoapOptionValue::ContentFormat, CoapContentType::ApplicationJson), result);
        response->SetPayload(output.dump(), result);
        response->SetCode(CoapMessageCode::Content, result);
    }
    else if(accept == CoapContentType::ApplicationCbor)
    {
        json output;
        output["ip"] = to_string(ipinfo.ip);
        output["mask"] = to_string(ipinfo.netmask);
        output["gateway"] = to_string(ipinfo.gw);

        response->AddOption(CoapUIntOption(CoapOptionValue::ContentFormat, CoapContentType::ApplicationCbor), result);
        response->SetPayload(json::to_cbor(output), result);
        response->SetCode(CoapMessageCode::Content, result);
    }
    else
    {
        response->SetCode(CoapMessageCode::BadOption, result);
        result = CoapResult::Error;
    }

}

WifiResource::WifiResource(ICoapInterface& coap)
    : _coap(coap)
{
    CoapResult result;
    this->_coap.CreateResource(this->_resource, this, "wifi", result);
    if (result != CoapResult::OK || this->_resource == nullptr)
    {
        ESP_LOGE( kTag, "CreateResource failed." );
        return;
    }

    this->_resource->RegisterHandler(CoapMessageCode::Get, result);
}
