#include "./relay_device_service.hpp"

#include "./relay_remote_device.hpp"

#include <pp_common/service_runtime.hpp>

static constexpr const size_t MAX_RELAY_REMOTE_DEVICE_PER_PROCESS = 10'0000;

static xel::xIndexedStorage<xRelayRemoteDevice *> RelayRemonteDevicePool;
static xel::xTcpServer                            RemoteDeviceServer;
static struct : xel::xTcpServer::iListener {
    void OnNewConnection(xTcpServer * TcpServerPtr, xSocket && NativeHandle) {}
} RemoteDeviceNewConnectionListener;

std::function<void()> RelayOnRemoteDeviceEnabled   = Noop<>;
std::function<void()> RelayOnRemoteDeviceKeepAlive = Noop<>;
std::function<void()> RelayOnRemoteDeviceRemoved   = Noop<>;

void EnableRelayRemoteDeviceService(const xel::xNetAddress & BindAddress) {
    RuntimeAssert(ServiceRunState, "EnableRelayRemoteDeviceService: ServiceRunState must be ensured");
    if (!RelayRemonteDevicePool.Init(MAX_RELAY_REMOTE_DEVICE_PER_PROCESS)) {
        Logger->F("RelayRemonteDevicePool failed to be inited");
    }
    if (!RemoteDeviceServer.Init(ServiceIoContext, BindAddress, &RemoteDeviceNewConnectionListener)) {
        Logger->F("RemoteDeviceServer(TcpServer) failed to be inited");
    }
}

void DisableRelayRemoteDeviceService() {
    RemoteDeviceServer.Clean();
    RelayRemonteDevicePool.Clean();
}
