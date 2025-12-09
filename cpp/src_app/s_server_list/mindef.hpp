#pragma once
#include <pp_common/_common.hpp>

using xServerListConnectionContext = struct {
    eServiceType Type;
    xServerInfo  Info;
};

using xServiceInfoByType = struct {
    xVersion                 Version = 1;
    std::vector<xServerInfo> List;

    bool   Dirty = true;
    ubyte  RespBuffer[MaxPacketSize];
    size_t RespSize = 0;
};