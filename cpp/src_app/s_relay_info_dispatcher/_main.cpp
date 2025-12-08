#include "../lib_util/server_bootstrap.hpp"

#include <pp_common/service_runtime.hpp>

xNetAddress MasterServerListServerAddress;
xNetAddress IncomeBindAddress4;
xNetAddress OutputBindAddress4;
xNetAddress ExportIncomeBindAddress4;
xNetAddress ExportOutputBindAddress4;

xTcpService IncomeService;
xTcpService OutputService;

xServerBootstrap Bootstrap;

int main(int argc, char ** argv) {

    X_VAR xServiceRuntimeEnvGuard(argc, argv);
    auto  LC = ServiceEnvironment.LoadConfig();

    LC.Require(MasterServerListServerAddress, "MasterServerListServerAddress");

    LC.Require(IncomeBindAddress4, "IncomeBindAddress4");
    LC.Require(OutputBindAddress4, "OutputBindAddress4");

    LC.Require(ExportIncomeBindAddress4, "ExportIncomeBindAddress4");
    LC.Require(ExportOutputBindAddress4, "ExportOutputBindAddress4");

    X_GUARD(IncomeService, ServiceIoContext, IncomeBindAddress4, 10'0000);
    X_GUARD(OutputService, ServiceIoContext, OutputBindAddress4, 10'0000);

    X_GUARD(Bootstrap);
    Bootstrap.SetMasterServerListServerAddress(MasterServerListServerAddress);
    Bootstrap.SetServerRegister({
        { eServiceType::RelayInfoDispatcher_RelayPort, ExportIncomeBindAddress4 },
        { eServiceType::RelayInfoDispatcher_ObserverPort, ExportOutputBindAddress4 },
    });

    while (ServiceRunState) {
        ServiceUpdateOnce(IncomeService, OutputService, Bootstrap);
    }

    return 0;
}
