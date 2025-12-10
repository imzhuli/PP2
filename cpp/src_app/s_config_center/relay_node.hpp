#pragma once
#include <pp_common/_common.hpp>
#include <pp_protocol/command.hpp>
#include <pp_protocol/device/challenge.hpp>

struct xCC_RelayV4Node : xListNode {};
using xCC_RelayV4List = xList<xCC_RelayV4Node>;

struct xCC_RelayV6Node : xListNode {};
using xCC_RelayV6List = xList<xCC_RelayV6Node>;

struct xRelayServerInfoBase {
    eRelayServerType ServerType;
    uint64_t         ServerId = {};
    uint64_t         StartupTimestampMS;
    xNetAddress      ExportProxyAddress4;

    xNetAddress ExportDeviceAddress4;
    xNetAddress ExportDeviceAddress6;

    uint32_t ForcedPoolId;

    std::string ToString() const;
};

struct xCC_RelayScheduleNode
    : xCC_RelayV4Node
    , xCC_RelayV6Node {
    const xRelayServerInfoBase * ServerInfo;
};
