/* net.cc
   Mathieu Stefani, 12 August 2015
   
*/

#include <pistache/net.h>
#include <pistache/common.h>
#include <pistache/config.h>

#include <stdexcept>
#include <limits>
#include <cstring>
#include <cassert>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <iostream>

namespace Pistache {

namespace {
    bool IsIPv4HostName(const std::string& host)
    {
        in_addr addr;
        char buff[INET_ADDRSTRLEN + 1] = {0, };
        std::copy(host.begin(), host.end(), buff);
        int res = inet_pton(AF_INET, buff, &addr);
        return res;
    }

    bool IsIPv6HostName(const std::string& host)
    {
        in6_addr addr6;
        char buff6[INET6_ADDRSTRLEN + 1] = {0, };
        std::copy(host.begin(), host.end(), buff6);
        int res = inet_pton(AF_INET6, buff6, &(addr6.s6_addr16));
        return res;
    }
}

Port::Port(uint16_t port)
    : port(port)
{}

Port::Port(const std::string& data)
{
    if (data.empty())
        throw std::invalid_argument("Invalid port: empty port");
    char *end = 0;
    long port_num = strtol(data.c_str(), &end, 10);
    if (*end != 0 || port_num < Port::min() || port_num > Port::max())
        throw std::invalid_argument("Invalid port: " + data);
    port = static_cast<uint16_t>(port_num);
}

bool
Port::isReserved() const {
    return port < 1024;
}

bool
Port::isUsed() const {
    throw std::runtime_error("Unimplemented");
    return false;
}

std::string
Port::toString() const {
    return std::to_string(port);
}

IP::IP() {
    family = AF_INET;
    addr = {0};
    addr.sin_family = AF_INET;
    uint8_t buff[INET_ADDRSTRLEN+1] = {0,0,0,0};
    memcpy(&addr.sin_addr.s_addr, buff, INET_ADDRSTRLEN);
}

IP::IP(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    family = AF_INET;
    addr = {0};
    addr.sin_family = AF_INET;
    uint8_t buff[INET_ADDRSTRLEN+1] = {a,b,c,d};
    memcpy(&addr.sin_addr.s_addr, buff, INET_ADDRSTRLEN);
}

IP::IP(uint16_t a, uint16_t b, uint16_t c, uint16_t d, uint16_t e, uint16_t f, uint16_t g, uint16_t h){
    family = AF_INET6;
    addr6 = { 0 };
    addr6.sin6_family = AF_INET6;
    uint16_t buff[9] {a,b,c,d,e,f,g,h, '\0'};
    uint16_t remap[9] = {0,0,0,0,0,0,0,0, '\0'};
    if ( htonl(1) != 1 ) {
        for (int i = 0; i<8; i++) {
            uint16_t x = buff[i];
            uint16_t y = htons(x);
            remap[i] = y;
        }
    } else {
        memcpy(&remap, &buff, sizeof(remap));
    }
    memcpy(&addr6.sin6_addr.s6_addr16, &remap, 8*sizeof(uint16_t));
}

IP::IP(struct sockaddr* _addr) {
    if (_addr->sa_family == AF_INET) { 
        struct sockaddr_in *in_addr = reinterpret_cast<struct sockaddr_in *>(_addr);
        family = AF_INET;
        port = in_addr->sin_port;
        memcpy(&(addr.sin_addr.s_addr), &(in_addr->sin_addr.s_addr), sizeof(in_addr_t));
    } else if (_addr->sa_family == AF_INET6) {
        struct sockaddr_in6 *in_addr = reinterpret_cast<struct sockaddr_in6 *>(_addr);
        family = AF_INET6;
        port = in_addr->sin6_port;
        memcpy(&(addr6.sin6_addr.s6_addr16), &(in_addr->sin6_addr.s6_addr16), 8*sizeof(uint16_t));
    }
}

IP
IP::any() {
    return IP(0, 0, 0, 0);
}

IP
IP::any(bool is_ipv6) {
    if (is_ipv6) {
        return IP(0, 0, 0, 0, 0, 0, 0, 0);
    } else {
        return IP(0, 0, 0, 0);
    }
}

IP
IP::loopback(){
    return IP(127, 0, 0, 1);
}

IP
IP::loopback(bool is_ipv6) {
    if (is_ipv6) {
        return IP(0, 0, 0, 0, 0, 0, 0, 1);
    } else {
        return IP(127, 0, 0, 1);
    }
}

int IP::getFamily() const {
    return family;
}

int IP::getPort() const {
    return port;
}

std::string
IP::toString() const {  
    char buff[INET6_ADDRSTRLEN+1];
    if (family == AF_INET) {        
        in_addr_t addr_;
        toNetwork(&addr_);
        inet_ntop(AF_INET, &addr_, buff, INET_ADDRSTRLEN);
    } else if (family == AF_INET6) {
        struct in6_addr addr_ = in6addr_any;
        toNetwork(&addr_);
        inet_ntop(AF_INET6, &addr_, buff, INET6_ADDRSTRLEN);
    }
    return std::string(buff);
}

void IP::toNetwork(in_addr_t *_addr) const {
    memcpy(_addr, &addr.sin_addr.s_addr, sizeof(uint32_t));
}

void IP::toNetwork(struct in6_addr *out) const {
    memcpy(&out->s6_addr16, &(addr6.sin6_addr.s6_addr16), 8*sizeof(uint16_t));
}

bool IP::supported() {
    struct ifaddrs *ifaddr = nullptr;
    struct ifaddrs *ifa = nullptr;
    int addr_family, n;
    bool supportsIpv6 = false;

    if (getifaddrs(&ifaddr) == -1) {
        throw std::runtime_error("Call to getifaddrs() failed");
    }

    for (ifa = ifaddr, n = 0; ifa != nullptr; ifa = ifa->ifa_next, n++) {
        if (ifa->ifa_addr == nullptr) {
            continue;
        }

        addr_family = ifa->ifa_addr->sa_family;
        if (addr_family == AF_INET6) {
            supportsIpv6 = true;
            continue;
        }
    }

    freeifaddrs(ifaddr);
    return supportsIpv6;
}


AddressParser::AddressParser(const std::string& data)
{
    std::size_t end_pos = data.find(']');
    std::size_t start_pos = data.find('[');
    if (start_pos != std::string::npos &&
        end_pos != std::string::npos &&
        start_pos < end_pos)
    {
        std::size_t colon_pos = data.find_first_of(':', end_pos);
        if (colon_pos != std::string::npos)
        {
            hasColon_ = true;
        }
        host_ = data.substr(start_pos, end_pos + 1);
        family_ = AF_INET6;
        ++end_pos;
    }
    else
    {
        std::size_t colon_pos = data.find(':');
        if (colon_pos != std::string::npos)
        {
            hasColon_ = true;
        }
        end_pos = colon_pos;
        host_ = data.substr(0, end_pos);
        family_ = AF_INET;
    }
    
    if (end_pos != std::string::npos && hasColon_)
    {
        port_ = data.substr(end_pos + 1);
        if (port_.empty())
            throw std::invalid_argument("Invalid port");
    }
}

const std::string& AddressParser::rawHost() const
{
    return host_;
}

const std::string& AddressParser::rawPort() const
{
    return port_;
}

bool AddressParser::hasColon() const
{
    return hasColon_;
}

int AddressParser::family() const
{
    return family_;
}

Address::Address() :
    ip_{}, port_{0}
{}

Address::Address(std::string host, Port port)
{   
    std::string addr = std::move(host);
    addr.append(":");
    addr.append(port.toString());
    init(std::move(addr));
}


Address::Address(std::string addr)
{
    init(std::move(addr));
}

Address::Address(const char* addr)
{
    init(std::string(addr));
}

Address::Address(IP ip, Port port)
    : ip_(ip)
    , port_(port)
{ }

Address
Address::fromUnix(struct sockaddr* addr) {
    if ((addr->sa_family==AF_INET) or (addr->sa_family==AF_INET6)) {
        IP ip = IP(addr);
        Port port = Port(ip.getPort());
        assert(addr);
        return Address(ip, port);
    }
    throw Error("Not an IP socket");    
}

std::string
Address::host() const {
    return ip_.toString();
}

Port
Address::port() const {
    return port_;
}

int
Address::family() const {
    return ip_.getFamily();
}



void Address::init(const std::string& addr)
{
    AddressParser parser(addr);
    int family_ = parser.family();
    std::string host_;
    if (family_ == AF_INET6)
    {
        const std::string& raw_host = parser.rawHost();
        assert(raw_host.size() > 2);
        host_ = addr.substr(1, raw_host.size() - 2);
        
        if (!IsIPv6HostName(host_))
        {
            throw std::invalid_argument("Invalid IPv6 address");
        }
        
        struct in6_addr addr;
        inet_pton(AF_INET6, host_.c_str(), &addr);
        struct sockaddr_in6 s_addr = {0};
        s_addr.sin6_family = AF_INET6;
        memcpy(&(s_addr.sin6_addr.s6_addr16), &addr.s6_addr16, 8*sizeof(uint16_t));
        ip_ = IP(reinterpret_cast<struct sockaddr *>(&s_addr));
    }
    else if (family_ == AF_INET)
    {
        host_ = parser.rawHost();
        
        if (host_ == "*")
        {
            host_ = "0.0.0.0";
        }
        else if (host_ == "localhost")
        {
            host_ = "127.0.0.1";
        }
        
        struct hostent *hp = ::gethostbyname(host_.c_str());
        
        if (hp)
        {
            struct in_addr **addr_list;
            addr_list = (struct in_addr **)hp->h_addr_list;
            for(int i = 0; addr_list[i] != NULL; i++)
            {
                // Just take the first IP address
                host_ = std::string(inet_ntoa(*addr_list[i]));
                break;
            }
        }
        
        if (!IsIPv4HostName(host_))
        {
            throw std::invalid_argument("Invalid IPv4 address");
        }
        
        struct in_addr addr;
        inet_pton(AF_INET, host_.c_str(), &addr);
        struct sockaddr_in s_addr = {0};
        s_addr.sin_family = AF_INET;
        memcpy(&(s_addr.sin_addr.s_addr), &addr.s_addr, sizeof(uint32_t));
        ip_ = IP(reinterpret_cast<struct sockaddr *>(&s_addr));
    }
    else
    { assert(false); }

    const std::string& portPart = parser.rawPort();
    if (portPart.empty())
    {
        if (parser.hasColon())
        {
            // "www.example.com:" or "127.0.0.1:" cases
            throw std::invalid_argument("Invalid port");
        }
        else
        {
            // "www.example.com" or "127.0.0.1" cases
            port_ = Const::HTTP_STANDARD_PORT;
        }
    }
    else
    {
        char *end = 0;
        long port = strtol(portPart.c_str(), &end, 10);
        if (*end != 0 || port < Port::min() || port > Port::max())
            throw std::invalid_argument("Invalid port");
        port_ = Port(port);
    }
}

Error::Error(const char* message)
    : std::runtime_error(message)
{ }

Error::Error(std::string message)
    : std::runtime_error(std::move(message))
{ }

Error
Error::system(const char* message) {
    const char *err = strerror(errno);

    std::string str(message);
    str += ": ";
    str += err;

    return Error(std::move(str));

}

} // namespace Pistache
