#include "./relay_local_binding_device.hpp"
#include <pp_common/service_runtime.hpp>

bool xRelayLocalBindingDevice::Init(const xNetAddress & LocalDevice) {
    assert(ServiceRunState);
    assert(LocalDevice && !LocalDevice.Port);

    BindAddress = LocalDevice;
    return false;
}

void xRelayLocalBindingDevice::Clean() {
    
}

