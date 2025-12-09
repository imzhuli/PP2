#include "../lib_client/server_list.hpp"
#include "./context_manager.hpp"
#include "./device_service.hpp"
#include "./pa_service.hpp"

#include <pp_common/service_runtime.hpp>

xNetAddress MasterServerListServerAddress;

xNetAddress BindAddressForDevice;
xNetAddress ExportAddressForDevice;

xNetAddress BindAddressForProxyAccess4;
xNetAddress ExportAddressForProxyAccess4;

static xServerListClient ServerListClient;

int main(int argc, char ** argv) {
    X_VAR xServiceEnvironmentGuard(argc, argv);
    auto  CL = ServiceEnvironment.LoadConfig();

    CL.Require(MasterServerListServerAddress, "MasterServerListServerAddress");
    CL.Require(BindAddressForDevice, "BindAddressForDevice");
    CL.Require(ExportAddressForDevice, "ExportAddressForDevice");
    CL.Require(BindAddressForProxyAccess4, "BindAddressForProxyAccess4");
    CL.Require(ExportAddressForProxyAccess4, "ExportAddressForProxyAccess4");

    RuntimeAssert(BindAddressForProxyAccess4.Is4() && BindAddressForProxyAccess4.Port);
    RuntimeAssert(ExportAddressForProxyAccess4.Is4() && ExportAddressForProxyAccess4.Port);

    X_VAR xScopeGuard([] { InitDeviceService(BindAddressForDevice); }, CleanDeviceService);
    X_VAR xScopeGuard([] { InitPAService(BindAddressForProxyAccess4); }, CleanPAService);
    X_VAR xScopeGuard([] { InitContextManager(); }, CleanContextManager);

    X_GUARD(ServerListClient, ServiceIoContext, std::vector{ MasterServerListServerAddress });
    ServerListClient.EnableDeviceStateRelayRelayPortUpdate(true);
    ServerListClient.EnableRelayInfoDispatcherRelayPortUpdate(true);
    ServerListClient.OnServerListUpdated = [](eServiceType Type, xVersion Version, const std::vector<xServerInfo> & List) {
        switch (Type) {
            case eServiceType::DeviceStateRelay_RelayPort:
                return UpdateDeviceStateRelayServerList(List);
            case eServiceType::RelayInfoDispatcher_RelayPort:
                return UpdateRelayInfoDispatcherServerList(List);
            default:
                break;
        }
    };

    while (ServiceRunState) {
        ServiceUpdateOnce(DeviceTicker, PAServiceTicker, ContextTicker, ServerListClient);
    }

    return 0;
}
