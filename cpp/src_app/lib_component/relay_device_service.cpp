#include "./relay_device_service.hpp"

#include "./relay_remote_device.hpp"

#include <pp_common/service_runtime.hpp>

static constexpr const size_t                       MAX_RELAY_REMOTE_DEVICE_PER_PROCESS        = 10'0000;
static constexpr const size_t                       MAX_RELAY_LOCAL_BINDING_DEVICE_PER_PROCESS = 1000;
static xel::xIndexedStorage<xRelayAbstractDevice *> DeviceIdManager;

static struct : xel::xTcpServer::iListener {
    void OnNewConnection(xTcpServer * TcpServerPtr, xSocket && NativeHandle) {}
} RemoteDeviceNewConnectionListener;
static xel::xTcpServer RemoteDeviceServer;
static bool            RemoteDeviceServerEnabled = false;

static bool LocalBindingeviceServerEnabled = false;

static auto DeviceIdManagerGuard = xel::xResourceGuard(DeviceIdManager, MAX_RELAY_REMOTE_DEVICE_PER_PROCESS + MAX_RELAY_LOCAL_BINDING_DEVICE_PER_PROCESS);
static auto UnitChecker          = xInstantRun([] {
    X_RUNTIME_ASSERT(DeviceIdManagerGuard);
});

////////////////// RelayRemoteDeviceService

std::function<void()> RelayOnRemoteDeviceEnabled   = Noop<>;
std::function<void()> RelayOnRemoteDeviceKeepAlive = Noop<>;
std::function<void()> RelayOnRemoteDeviceRemoved   = Noop<>;

void EnableRelayRemoteDeviceService(const xel::xNetAddress & BindAddress) {
    X_RUNTIME_ASSERT(!RemoteDeviceServerEnabled);
    X_RUNTIME_ASSERT(ServiceRunState);
    if (!RemoteDeviceServer.Init(ServiceIoContext, BindAddress, &RemoteDeviceNewConnectionListener)) {
        Logger->F("RemoteDeviceServer(TcpServer) failed to be inited");
    }
    RemoteDeviceServerEnabled = true;
}

void DisableRelayRemoteDeviceService() {
    X_RUNTIME_ASSERT(RemoteDeviceServerEnabled);
    RemoteDeviceServer.Clean();
    RemoteDeviceServerEnabled = false;
}

////////// LocalBindingDevice

void EnableRelayLocalBindingDeviceService(const char * LocalBindingFilename) {
    X_RUNTIME_ASSERT(!LocalBindingeviceServerEnabled);
    LocalBindingeviceServerEnabled = true;
}

void DisableRelayLocaBindinglDeviceService() {
    X_RUNTIME_ASSERT(LocalBindingeviceServerEnabled);
    LocalBindingeviceServerEnabled = false;
}

xRelayLocalBindingDevice * CreateLocalBindingDevice(const xNetAddress & LocalBindingAddress) {
    auto Id = DeviceIdManager.Acquire();
    if (!Id) {
        return nullptr;
    }
    auto Device = new xRelayLocalBindingDevice();
    if (!Device->Init(LocalBindingAddress)) {
        DeviceIdManager.Release(Id);
        return nullptr;
    }
    Device->DeviceId = Id;
    DeviceIdManager[Id] = Device;
    return Device;
}

void ReleaseLocalBindingDevice(xRelayLocalBindingDevice * Device) {
#ifndef NDEBUG
    X_RUNTIME_ASSERT(Device && Device->DeviceId);
    auto PPD = DeviceIdManager.CheckAndGet(Device->DeviceId);
    X_RUNTIME_ASSERT(PPD && *PPD == Device);
#endif
    DeviceIdManager.Release(Device->DeviceId);
    Device->Clean();
    delete Device;
}
