#include "../lib_component/relay_local_binding_device.hpp"

#include <pp_common/service_runtime.hpp>

int main(int argc, char ** argv) {
    X_VAR xServiceEnvironmentGuard(argc, argv);

    auto D = CreateLocalBindingDevice(xNetAddress::Parse("10.0.0.7"));
    X_SCOPE_GUARD([&] { ReleaseLocalBindingDevice(D); });

    auto C = D->CreateConnection(xNetAddress::Parse("127.0.0.1:12000"));
    X_SCOPE_GUARD([&] { D->DestroyConnection(C); });

    while (ServiceRunState) {
        ServiceUpdateOnce();
    }

    return 0;
}