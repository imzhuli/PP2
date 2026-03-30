#include "../lib_component/pa_local.hpp"
#include "../lib_component/relay_local_binding_device.hpp"

#include <pp_common/service_runtime.hpp>

int main(int argc, char ** argv) {
    X_VAR xServiceEnvironmentGuard(argc, argv);

    while (ServiceRunState) {
        ServiceUpdateOnce();
    }
}