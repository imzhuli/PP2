#include "../lib_client/server_list.hpp"
#include "../lib_util/server_bootstrap.hpp"
#include "./context_manager.hpp"
#include "./device_service.hpp"
#include "./global.hpp"
#include "./pa_service.hpp"
#include "./relay_report.hpp"

#include <pp_common/service_runtime.hpp>

int main(int argc, char ** argv) {
    X_VAR xServiceEnvironmentGuard(argc, argv);
    auto  CL = ServiceEnvironment.LoadConfig();

    CL.Require(MasterServerListServerAddress, "MasterServerListServerAddress");
    CL.Require(BindAddressForDevice, "BindAddressForDevice");
    CL.Require(ExportAddressForDevice, "ExportAddressForDevice");
    CL.Require(BindAddressForProxyAccess4, "BindAddressForProxyAccess4");
    CL.Require(ExportAddressForProxyAccess4, "ExportAddressForProxyAccess4");

    SERVICE_RUNTIME_ASSERT(BindAddressForProxyAccess4.Is4() && BindAddressForProxyAccess4.Port);
    SERVICE_RUNTIME_ASSERT(ExportAddressForProxyAccess4.Is4() && ExportAddressForProxyAccess4.Port);

    X_VAR xScopeGuard([] { InitDeviceService(BindAddressForDevice); }, CleanDeviceService);
    X_VAR xScopeGuard([] { InitPAService(BindAddressForProxyAccess4); }, CleanPAService);
    X_VAR xScopeGuard(InitContextManager, CleanContextManager);
    X_VAR xScopeGuard(InitRelayReport, CleanRelayReport);

    X_RESOURCE_GUARD(Bootstrap, MasterServerListServerAddress);
    Bootstrap.EnableDeviceStateRelayRelayPortUpdate(true);
    Bootstrap.EnableRelayInfoDispatcherRelayPortUpdate(true);
    Bootstrap.OnServerListUpdated = [](eServiceType Type, xVersion Version, std::vector<xServerInfo> && List) {
        switch (Type) {
            case eServiceType::DeviceStateRelay_RelayPort:
                return UpdateDeviceStateRelayServerList(List);
            case eServiceType::RelayInfoDispatcher_RelayPort:
                return UpdateRelayInfoDispatcherServerList(std::move(List));
            default:
                break;
        }
    };

    while (ServiceRunState) {
        ServiceUpdateOnce(DeviceTicker, PAServiceTicker, ContextTicker, TickRelayReport, Bootstrap);
    }

    return 0;
}
