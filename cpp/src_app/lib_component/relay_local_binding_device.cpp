#include "./relay_local_binding_device.hpp"

#include "./relay_device_service.hpp"

#include <pp_common/service_runtime.hpp>

class xLocalBindingConnection : public xel::xTcpConnection {
};

bool xRelayLocalBindingDevice::Init(const xNetAddress & LocalDevice) {
    assert(ServiceRunState);
    assert(LocalDevice && !LocalDevice.Port);

    BindAddress = LocalDevice;
    return false;
}

void xRelayLocalBindingDevice::Clean() {
}

xRelayLocalBindingDevice * CreateLocalBindingDevice(const xNetAddress & LocalBindingAddress) {
    auto Device = new xRelayLocalBindingDevice();
    if (!Device->Init(LocalBindingAddress)) {
        return nullptr;
    }
    if (!(Device->DeviceId = AcquireDeviceId(Device))) {
        Device->Clean();
        delete Device;
        return nullptr;
    }
    return Device;
}

void ReleaseLocalBindingDevice(xRelayLocalBindingDevice * Device) {
    ReleaseDeviceId(Device->DeviceId);
    Device->Clean();
    delete Device;
}

uint64_t xRelayLocalBindingDevice::CreateConnection(const std::string & TargetHostname, uint16_t Port) {
    return 0;
}

uint64_t xRelayLocalBindingDevice::CreateConnection(const xel::xNetAddress & TargetAddress) {
    return 0;
}

void xRelayLocalBindingDevice::DestroyConnection(uint64_t ConnectionId) {
}

uint64_t xRelayLocalBindingDevice::CreateUdpChannel() {
    return 0;
}

void xRelayLocalBindingDevice::DestroyUdpChannel(uint64_t ChannelId) {
}

std::vector<xRelayLocalBindingDevice *> GetLoadLocalBindingDeviceList();
