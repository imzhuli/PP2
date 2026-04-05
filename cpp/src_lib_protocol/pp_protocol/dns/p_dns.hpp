#pragma once
#include <pp_common/_.hpp>

struct xPP_DnsQuery : public xBinaryMessage {  // from proxy_access to relay server
public:
    void SerializeMembers() override {
        W(Hostname);  //
        W(Ipv6);
    }

    void DeserializeMembers() override {
        R(Hostname);  //
        R(Ipv6);
    }

    std::string_view Hostname;
    bool             Ipv6;
};

struct xPP_DnsQueryResp : public xBinaryMessage {  // from proxy_access to relay server
public:
    void SerializeMembers() override {
        W(Hostname);
        W(Address);
    }
    void DeserializeMembers() override {
        R(Hostname);
        R(Address);
    }

    std::string_view Hostname;
    xNetAddress      Address;
};
