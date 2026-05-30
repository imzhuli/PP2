#include "../lib_component/server_id_service.hpp"

#include <pp_common/service_runtime.hpp>

static auto BindAddress     = xNetAddress();
static auto ServerIdService = xServerIdService();

int main(int argc, char ** argv) {

    X_VAR xServiceEnvironmentGuard(argc, argv);
    auto  CL = ServiceEnvironment.LoadConfig();
    CL.Require(BindAddress, "BindAddress");

    X_RESOURCE_GUARD_ASSERTED(ServerIdService, BindAddress);
    ServerIdService.EnableServerType(ST_SERVER_LIST);
    ServerIdService.EnableServerType(ST_TARGET_COLLECTOR);

    while (ServiceRunState) {
        ServiceUpdateOnce(ServerIdService);
    }

    return 0;
}
