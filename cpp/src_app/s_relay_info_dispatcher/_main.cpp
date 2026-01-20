#include "../lib_util/server_bootstrap.hpp"
#include "./config.hpp"
#include "./observer_port.hpp"
#include "./relay_port.hpp"

#include <pp_common/service_runtime.hpp>

xServerBootstrap Bootstrap;

int main(int argc, char ** argv) {

    X_VAR xServiceEnvironmentGuard(argc, argv);
    LoadConfig();

    X_RESOURCE_GUARD(Bootstrap, MasterServerListServerAddress);
    Bootstrap.SetServerRegister({
        { eServiceType::RelayInfoDispatcher_RelayPort, RelayPortExportAddress4 },
        { eServiceType::RelayInfoDispatcher_ObserverPort, ObserverExportAddress4 },
    });

    X_VAR xScopeGuard(InitRelayPort, CleanRelayPort);
    X_VAR xScopeGuard(InitObserverPort, CleanObserverPort);

    while (ServiceRunState) {
        ServiceUpdateOnce(RelayPortTick, ObserverPortTick, Bootstrap);
    }

    return 0;
}
