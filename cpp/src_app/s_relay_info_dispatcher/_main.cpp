#include "../lib_util/server_bootstrap.hpp"

#include <pp_common/service_runtime.hpp>

xNetAddress MasterServerListServerAddress;
xNetAddress BindInputAddress4;
xNetAddress BindOutputAddress4;
xNetAddress ExportInputAddress4;
xNetAddress ExportOutputAddress4;

xTcpService IncomeService;
xTcpService OutputService;

xServerBootstrap Bootstrap;

int main(int argc, char ** argv) {

    X_VAR xServiceEnvironmentGuard(argc, argv);
    auto  LC = ServiceEnvironment.LoadConfig();

    LC.Require(MasterServerListServerAddress, "MasterServerListServerAddress");

    LC.Require(BindInputAddress4, "BindInputAddress4");
    LC.Require(BindOutputAddress4, "BindOutputAddress4");

    LC.Require(ExportInputAddress4, "ExportInputAddress4");
    LC.Require(ExportOutputAddress4, "ExportOutputAddress4");

    X_GUARD(IncomeService, ServiceIoContext, BindInputAddress4, 10'0000);
    X_GUARD(OutputService, ServiceIoContext, BindOutputAddress4, 10'0000);

    X_GUARD(Bootstrap);
    Bootstrap.SetMasterServerListServerAddress(MasterServerListServerAddress);
    Bootstrap.SetServerRegister({
        { eServiceType::RelayInfoDispatcher_RelayPort, ExportInputAddress4 },
        { eServiceType::RelayInfoDispatcher_ObserverPort, ExportOutputAddress4 },
    });

    while (ServiceRunState) {
        ServiceUpdateOnce(IncomeService, OutputService, Bootstrap);
    }

    return 0;
}
