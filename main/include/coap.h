#ifndef __MAIN_COAP_
#define __MAIN_COAP_

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <tuple>
#include <type_traits>
#include <vector>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

enum class CoapResult
{
    OK = 0,
    Error,
    Postpone,
};

// TODO: Adjust these max sizes
enum CoapConstraints : unsigned long long
{
    MaxResourceSize = 50,
    MaxMessageSize = 10,
    MaxOptionSize = 10,
};

enum CoapContentType : int {
    TextPlain = 0,
	LinkFormat = 40,
	ApplicationXml = 41,
	ApplicationOctetStream = 42,
	ApplicationExi = 47,
	ApplicationJson = 50,
	ApplicationCbor = 60
};

enum CoapOptionValue : int {
    IfMatch = 1,
    UriHost = 3,
    ETag = 4,
    IfNoneMatch = 5,
    UriPort = 7,
    LocationPath = 8,
    UriPath = 11,
    ContentFormat = 12,
    MaxAge = 14,
    UriQuery = 15,
    Accept = 17,
    LocationQuery = 20,
    ProxyUri = 35,
    ProxyScheme = 39,
    Size1 = 60,
};

enum class CoapOptionType
{
    Empty,
    Opaque,
    UInt,
    String,
};

extern std::tuple<CoapOptionValue, CoapOptionType> const CoapOptiontypeMap[];

#define MESSAGE_CODE_FROM_CLASS_CODE(CLASS, CODE) ( (CLASS <<5) | CODE )
enum CoapMessageCode : int {
    None = 0,
    // 0.xx Request
    Get = 1,
    Post = 2,
    Put = 3,
    Delete = 4,
    // 2.xx Success
    Created = MESSAGE_CODE_FROM_CLASS_CODE( 2, 01),
    Deleted = MESSAGE_CODE_FROM_CLASS_CODE( 2, 02),
    Valid   = MESSAGE_CODE_FROM_CLASS_CODE( 2, 03),
    Changed = MESSAGE_CODE_FROM_CLASS_CODE( 2, 04),
    Content = MESSAGE_CODE_FROM_CLASS_CODE( 2, 05),
    // 4.xx Client Error
    BadRequest               = MESSAGE_CODE_FROM_CLASS_CODE( 4, 00),
    Unauthorized             = MESSAGE_CODE_FROM_CLASS_CODE( 4, 01),
    BadOption                = MESSAGE_CODE_FROM_CLASS_CODE( 4, 02),
    Forbidden                = MESSAGE_CODE_FROM_CLASS_CODE( 4, 03),
    NotFound                 = MESSAGE_CODE_FROM_CLASS_CODE( 4, 04),
    MethodNotAllowed         = MESSAGE_CODE_FROM_CLASS_CODE( 4, 05),
    NotAcceptable            = MESSAGE_CODE_FROM_CLASS_CODE( 4, 06),
    PreconditionFailed       = MESSAGE_CODE_FROM_CLASS_CODE( 4, 12),
    RequestEntityTooLarge    = MESSAGE_CODE_FROM_CLASS_CODE( 4, 13),
    UnsupportedContentFormat = MESSAGE_CODE_FROM_CLASS_CODE( 4, 15),
    // 5.xx Server Error
    InternalServerError  = MESSAGE_CODE_FROM_CLASS_CODE( 5, 00),
    NotImplemented       = MESSAGE_CODE_FROM_CLASS_CODE( 5, 01),
    BadGateway           = MESSAGE_CODE_FROM_CLASS_CODE( 5, 02),
    ServiceUnavailable   = MESSAGE_CODE_FROM_CLASS_CODE( 5, 03),
    GatewayTimeout       = MESSAGE_CODE_FROM_CLASS_CODE( 5, 04),
    ProxyingNotSupported = MESSAGE_CODE_FROM_CLASS_CODE( 5, 05)
};

template<class TInterface, int MaxSize>
class StackAllocator
{
    char _allocation[MaxSize];
public:
    TInterface *operator->()
    {
        return (TInterface*)_allocation;
    }
    TInterface const *operator->() const
    {
        return (TInterface const *)_allocation;
    }

    TInterface *get() { return (TInterface *)_allocation; }
    template<class T>
    T *get() { return (T *)_allocation; }

    operator TInterface*() { return (TInterface *)_allocation; }
    operator TInterface const *() const { return (TInterface const *)_allocation; }
};

struct CoapDtlsOptions
{
    const unsigned char *cert_ptr;
    size_t cert_len;

    const unsigned char *cert_key_ptr;
    size_t cert_key_len;
};

struct CoapOptions
{
    struct {
        unsigned char useDTLS: 1;
    } flags;
    CoapDtlsOptions DTLS;
};

typedef void* CoapOption_t;
typedef void* CoapMessage_t;

class ICoapResource;
class ICoapMessage;
class ICoapOption;
using CoapResource = StackAllocator<ICoapResource, CoapConstraints::MaxResourceSize>;
using CoapMessage = StackAllocator<ICoapMessage, CoapConstraints::MaxMessageSize>;
using CoapOption = StackAllocator<ICoapOption, CoapConstraints::MaxOptionSize>;

using Payload = std::basic_string<uint8_t>;

class IApplicationResource
{
public:
    virtual void HandleRequest(ICoapMessage const *request, ICoapMessage *response, CoapResult &result) { result = CoapResult::Error; };
    virtual ~IApplicationResource() {};
};

class ICoapInterface {
public:
    virtual ~ICoapInterface() {};

    // CoapResult option_get_next( CoapOption_t* option );
    // CoapResult option_get_uint( const CoapOption_t option, uint32_t* value );

    virtual void Start(CoapResult &result) = 0;
    virtual void CreateResource(CoapResource &resource, IApplicationResource * const applicationResource, const char* uri, CoapResult &result) = 0;

    virtual void SetNetworkReady(bool ready) = 0;
};

class ICoapMessage
{
public:
    virtual ~ICoapMessage(){}
    // virtual void GetOption_uint(const uint16_t option, uint32_t *value, CoapResult &result);
    // virtual void AddOption_uint(uint16_t option, uint32_t code, CoapResult &result);
    virtual void GetOption(CoapOption &option,const uint16_t number, CoapResult &result) const = 0;
    virtual void AddOption(ICoapOption const *option, CoapResult &result) = 0;
    virtual void AddOption(ICoapOption const &option, CoapResult &result) { this->AddOption(&option, result); }
    virtual CoapMessageCode GetCode() const = 0;
    virtual void SetCode(CoapMessageCode code, CoapResult &result) = 0;
    virtual void GetPayload(Payload &payload, CoapResult &result) const = 0;
    virtual void SetPayload(Payload const &payload, CoapResult &result) = 0;

    template<class T>
    void SetPayload(std::vector<T> const &something, CoapResult &result) { this->SetPayload(Payload(something.begin(), something.end()), result); }
    template<class T>
    void SetPayload(T const &something, CoapResult &result) { this->SetPayload(Payload((const uint8_t *)something.data(), something.length()), result); }
    void SetPayload(const char *something, CoapResult &result) { this->SetPayload(Payload((const uint8_t *)something), result); }
};

class ICoapResource
{
protected:
    IApplicationResource * const applicationResource;
public:
    ICoapResource(IApplicationResource * const applicationResource)
        : applicationResource(applicationResource) {}
    virtual void RegisterHandler(CoapMessageCode requestType, CoapResult &result) = 0;

    void RegisterHandler(CoapResult &result)
    {
        this->RegisterHandler(CoapMessageCode::Get, result);
    }

    virtual ~ICoapResource() {}
};

class ICoapOption
{
public:
    uint16_t const Number;
    CoapOptionType const Type;

    ICoapOption(CoapOptionType type, uint16_t number)
        : Number(number), Type(type){}
    virtual ~ICoapOption(){}

    virtual size_t GetSize() const = 0;
    virtual void const * GetPtr() const = 0;
};

class CoapEmptyOption : public ICoapOption
{
public:
    CoapEmptyOption(uint16_t number)
        : ICoapOption(CoapOptionType::Empty, number) {}
    ~CoapEmptyOption(){}

    size_t GetSize() const { return 0; }
    void const * GetPtr() const {return nullptr; }
};

class CoapOpaqueOption : public ICoapOption
{
public:
    Payload Data;
    CoapOpaqueOption(uint16_t number)
        : ICoapOption(CoapOptionType::Opaque, number) {}
    CoapOpaqueOption(uint16_t number, const Payload &payload)
        : ICoapOption(CoapOptionType::Opaque, number), Data(payload) {}
    ~CoapOpaqueOption(){}

    size_t GetSize() const { return Data.length(); }
    void const * GetPtr() const { return Data.data(); }
};

class CoapStringOption : public ICoapOption
{
public:
    std::string Data;
    CoapStringOption(uint16_t number)
        : ICoapOption(CoapOptionType::String, number) {}
    CoapStringOption(uint16_t number, const std::string &data)
        : ICoapOption(CoapOptionType::String, number), Data(data) {}
    ~CoapStringOption(){}

    size_t GetSize() const { return Data.length(); }
    void const * GetPtr() const { return Data.data(); }
};

class CoapUIntOption : public ICoapOption
{
public:
    uint32_t Value;
    CoapUIntOption(uint16_t number)
        : ICoapOption(CoapOptionType::UInt, number) {}
    CoapUIntOption(uint16_t number, uint32_t value)
        : ICoapOption(CoapOptionType::UInt, number), Value(value) {}
    ~CoapUIntOption(){}

    size_t GetSize() const
    {
        if (this->Value > 0xFFFFFFu)
            return 4;
        else if (this->Value > 0xFFFFu)
            return 3;
        else if (this->Value > 0xFFu)
            return 2;
        else if (this->Value > 0u)
            return 1;
        else
            return 0;
    }
    void const * GetPtr() const
    {
        // TODO: figure this endian shit out
        return nullptr;
    }
};

inline CoapEmptyOption *AsEmpty(ICoapOption &option) { assert(option.Type == CoapOptionType::Empty); return static_cast<CoapEmptyOption*>(&option);}
inline CoapEmptyOption *AsEmpty(ICoapOption *option) { assert(option->Type == CoapOptionType::Empty); return static_cast<CoapEmptyOption*>(option);}

inline CoapOpaqueOption *AsOpaque(ICoapOption &option) { assert(option.Type == CoapOptionType::Opaque); return static_cast<CoapOpaqueOption*>(&option);}
inline CoapOpaqueOption *AsOpaque(ICoapOption *option) { assert(option->Type == CoapOptionType::Opaque); return static_cast<CoapOpaqueOption*>(option);}

inline CoapStringOption *AsString(ICoapOption &option) { assert(option.Type == CoapOptionType::String); return static_cast<CoapStringOption*>(&option);}
inline CoapStringOption *AsString(ICoapOption *option) { assert(option->Type == CoapOptionType::String); return static_cast<CoapStringOption*>(option);}

inline CoapUIntOption *AsUInt(ICoapOption &option) { assert(option.Type == CoapOptionType::UInt); return static_cast<CoapUIntOption*>(&option);}
inline CoapUIntOption *AsUInt(ICoapOption *option) { assert(option->Type == CoapOptionType::UInt); return static_cast<CoapUIntOption*>(option);}

#endif /* __MAIN_COAP_ */
