#include "./device_service.hpp"

#include "../lib_util/service_client_pool.hpp"

#include <pp_common/service_runtime.hpp>

static xPPClientPool DeviceReportClientPool;

static std::vector<xServerInfo> DeviceReportServerInfoList;
static std::vector<xServerInfo> TempAddServerInfoList;
static std::vector<xServerInfo> TempRemoveServerInfoList;

static xTcpService DeviceService;

void InitDeviceService(const xNetAddress & BindAddress) {
    SERVICE_RUNTIME_ASSERT(DeviceService.Init(ServiceIoContext, BindAddress, MAX_DEVICE_CONNECTION_ON_RELAY));

    RuntimeAssert(DeviceReportClientPool.Init(MAX_DEVICE_STATE_RELAY_SERVER_COUNT));
    SERVICE_RUNTIME_ASSERT(DeviceReportServerInfoList.empty());
    DeviceReportServerInfoList.reserve(std::max(MAX_DEVICE_STATE_RELAY_SERVER_COUNT, MAX_RELAY_INFO_DISPATCHER_SERVER_COUNT));

    TempAddServerInfoList.reserve(MAX_DEVICE_STATE_RELAY_SERVER_COUNT);
    TempRemoveServerInfoList.reserve(MAX_DEVICE_STATE_RELAY_SERVER_COUNT);
}

void CleanDeviceService() {
    DeviceReportClientPool.Clean();
    Reset(DeviceReportServerInfoList);

    Reset(TempAddServerInfoList);
    Reset(TempRemoveServerInfoList);

    DeviceService.Clean();
}

void DeviceTicker(uint64_t NowMS) {
    TickAll(NowMS, DeviceReportClientPool);
}

void UpdateDeviceStateRelayServerList(const std::vector<xServerInfo> & ServerList) {
    DeviceReportClientPool.UpdateServerList(ServerList);
}
