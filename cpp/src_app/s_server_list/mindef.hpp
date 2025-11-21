#pragma once
#include <pp_common/_common.hpp>

using xServerListConnectionContext = struct {
    eServiceType Type;
    xServiceInfo Info;
};

using xServiceInfoByType = struct {
    xVersion                  Version;
    bool                      Dirty;
    std::vector<xServiceInfo> List;

    ubyte  RespBuffer[MaxPacketSize];
    size_t RespSize = 0;
};