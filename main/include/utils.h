#ifndef _MAIN_UTILS_H_
#define _MAIN_UTILS_H_

#include <sstream>
#include <ostream>

#include "tcpip_adapter.h"

template<typename TChar, typename TTraits>
inline std::basic_ostream<TChar, TTraits>& operator<<(std::basic_ostream<TChar, TTraits> &os, const ip4_addr_t &ip)
{
    os << ip4_addr1_16(&ip) << "." << ip4_addr2_16(&ip) << "." << ip4_addr3_16(&ip) << "." << ip4_addr4_16(&ip);
    return os;
}

inline std::string to_string(const ip4_addr_t &ip)
{
    std::string output;
    output += std::to_string(ip4_addr1_16(&ip)) + "." + std::to_string(ip4_addr2_16(&ip)) + "." + std::to_string(ip4_addr3_16(&ip)) + "." + std::to_string(ip4_addr4_16(&ip));
    return output;
}

#endif // _MAIN_UTILS_H_
