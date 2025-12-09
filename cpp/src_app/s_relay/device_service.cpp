#include "./device_service.hpp"

#include <pp_common/service_runtime.hpp>

static xel::xClientPool SelfReportClientPool;
static xel::xClientPool

    void
    InitDeviceService(const xNetAddress & BindAddress) {
}

void CleanDeviceService() {
}

void DeviceTicker(uint64_t NowMS) {
}

void UpdateDeviceStateRelayServerList(const std::vector<xServiceInfo> & ServerList) {
    auto OS = std::ostringstream();
    for (auto & SI : ServerList) {
        OS << SI.Address.ToString() << " ";
    }
    Logger->I("Update device state relay server list: %s", OS.str().c_str());
}

void UpdateRelayInfoDispatcherServerList(const std::vector<xServiceInfo> & ServerList) {
    auto OS = std::ostringstream();
    for (auto & SI : ServerList) {
        OS << SI.Address.ToString() << " ";
    }
    Logger->I("Update relay info dispatcher server list: %s", OS.str().c_str());
}
