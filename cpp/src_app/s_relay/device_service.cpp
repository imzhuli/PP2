#include "./device_service.hpp"

#include "../lib_util/service_client_pool.hpp"

#include <pp_common/service_runtime.hpp>

static xPPClientPool DeviceReportClientPool;
static xPPClientPool SelfReportClientPool;

static std::vector<xServerInfo> DeviceReportServerInfoList;
static std::vector<xServerInfo> SelfReportServerInfoList;

static std::vector<xServerInfo> TempAddServerInfoList;
static std::vector<xServerInfo> TempRemoveServerInfoList;

void InitDeviceService(const xNetAddress & BindAddress) {
    assert(DeviceReportServerInfoList.empty());
    assert(SelfReportServerInfoList.empty());

    RuntimeAssert(DeviceReportClientPool.Init(MAX_DEVICE_STATE_RELAY_SERVER_COUNT));
    RuntimeAssert(SelfReportClientPool.Init(MAX_RELAY_INFO_DISPATCHER_SERVER_COUNT));

    DeviceReportServerInfoList.reserve(std::max(MAX_DEVICE_STATE_RELAY_SERVER_COUNT, MAX_RELAY_INFO_DISPATCHER_SERVER_COUNT));
    SelfReportServerInfoList.reserve(std::max(MAX_DEVICE_STATE_RELAY_SERVER_COUNT, MAX_RELAY_INFO_DISPATCHER_SERVER_COUNT));

    TempAddServerInfoList.reserve(std::max(MAX_DEVICE_STATE_RELAY_SERVER_COUNT, MAX_RELAY_INFO_DISPATCHER_SERVER_COUNT));
    TempRemoveServerInfoList.reserve(std::max(MAX_DEVICE_STATE_RELAY_SERVER_COUNT, MAX_RELAY_INFO_DISPATCHER_SERVER_COUNT));
}

void CleanDeviceService() {
    DeviceReportClientPool.Clean();
    SelfReportClientPool.Clean();

    Reset(DeviceReportServerInfoList);
    Reset(SelfReportServerInfoList);

    Reset(TempAddServerInfoList);
    Reset(TempRemoveServerInfoList);
}

void DeviceTicker(uint64_t NowMS) {
    TickAll(NowMS, DeviceReportClientPool, SelfReportClientPool);
}

void UpdateDeviceStateRelayServerList(const std::vector<xServerInfo> & ServerList) {
    DeviceReportClientPool.UpdateServerList(ServerList);
}

void UpdateRelayInfoDispatcherServerList(const std::vector<xServerInfo> & ServerList) {
    SelfReportClientPool.UpdateServerList(ServerList);
}
