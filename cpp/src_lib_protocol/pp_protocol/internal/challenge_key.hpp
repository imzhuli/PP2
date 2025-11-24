#pragma once
#include <pp_common/_.hpp>
#include <pp_common/_region.hpp>

struct xDeviceChallengePack {
    uint32_t    Version;
    uint32_t    ChannelId;
    xNetAddress Tcp4Address;
    xNetAddress Tcp6Address;
    xNetAddress Udp4Address;
    xNetAddress Udp6Address;
    xGeoInfo    GeoInfo;
};

extern std::string          MakeChallengeKey(const xDeviceChallengePack & AddressPack);
extern xDeviceChallengePack ExtractChallengeKey(const std::string & Key);
