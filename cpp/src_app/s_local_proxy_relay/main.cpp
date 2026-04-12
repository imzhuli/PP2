#include "../lib_component/auth_local.hpp"
#include "../lib_component/pa_local.hpp"
#include "../lib_component/relay_local_binding_device.hpp"

#include <pp_common/service_runtime.hpp>

static auto LocalAuthService = xAuthLocalService();

int main(int argc, char ** argv) {
    X_VAR xServiceEnvironmentGuard(argc, argv);
    X_RESOURCE_GUARD(LocalAuthService, "/tmp/test.dir");

    while (ServiceRunState) {
        ServiceUpdateOnce();
    }
}