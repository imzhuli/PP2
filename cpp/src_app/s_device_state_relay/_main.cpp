#include "../lib_util/server_bootstrap.hpp"
#include "./relay.hpp"

#include <pp_common/_.hpp>
#include <pp_common/service_runtime.hpp>
#include <pp_protocol/command.hpp>

using namespace xel;

static xNetAddress MasterServerListServerAddress;
static xNetAddress BindInputAddress4;
static xNetAddress BindOutputAddress4;
static xNetAddress ExportInputAddress4;
static xNetAddress ExportOutputAddress4;

static xServerBootstrap Bootstrap;

static void LoadConfig() {
    auto LC = ServiceEnvironment.LoadConfig();
    LC.Require(MasterServerListServerAddress, "MasterServerListServerAddress");

    LC.Require(BindInputAddress4, "BindInputAddress4");
    LC.Require(BindOutputAddress4, "BindOutputAddress4");
    LC.Require(ExportInputAddress4, "ExportInputAddress4");
    LC.Require(ExportOutputAddress4, "ExportOutputAddress4");

    RuntimeAssert(BindInputAddress4.Is4() && BindInputAddress4.Port);
    RuntimeAssert(BindOutputAddress4.Is4() && BindOutputAddress4.Port);
    RuntimeAssert(ExportInputAddress4.Is4() && ExportInputAddress4.Port);
    RuntimeAssert(ExportOutputAddress4.Is4() && ExportOutputAddress4.Port);
}

int main(int argc, char ** argv) {
    X_VAR xServiceEnvironmentGuard(argc, argv);
    LoadConfig();

    X_RESOURCE_GUARD(Bootstrap, MasterServerListServerAddress);
    Bootstrap.SetServerRegister({
        { eServiceType::DeviceStateRelay_RelayPort, ExportInputAddress4 },
        { eServiceType::DeviceStateRelay_ObserverPort, ExportOutputAddress4 },
    });

    X_VAR xScopeGuard([] { InitRelayService(BindInputAddress4, BindOutputAddress4); }, CleanRelayService);

    while (true) {
        ServiceUpdateOnce(Bootstrap, RelayTicker);
    }

    return 0;
}
