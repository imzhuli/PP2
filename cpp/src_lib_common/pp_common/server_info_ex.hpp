#pragma once
#include "./_.hpp"

struct xRelayServerDevicePortInfo {
    xNetAddress Address4;
    xNetAddress Address6;
};

struct xRelayServerProxyPortInfo {
    uint32_t    RelayServerGroupId;
    uint64_t    RelayServerId;
    xNetAddress Address4;
};
