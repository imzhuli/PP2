#include "./relay_local_binding_device.hpp"

#include "./relay_device_service.hpp"

#include <pp_common/service_runtime.hpp>

bool xRelayLocalBindingDevice::Init(const xNetAddress & LocalDevice) {
    assert(ServiceRunState);
    assert(LocalDevice && !LocalDevice.Port);

    BindAddress = LocalDevice;
    return false;
}

void xRelayLocalBindingDevice::Clean() {
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
    Device->DeviceId    = Id;
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

std::vector<xRelayLocalBindingDevice *> GetLoadLocalBindingDeviceList();