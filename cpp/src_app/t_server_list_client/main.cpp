#include "../lib_client/server_list.hpp"
#include "../lib_client/server_register.hpp"

#include <pp_common/service_runtime.hpp>

xServerListClient SLC;

int main(int argc, char ** argv) {
    X_VAR xServiceRuntimeEnvGuard(argc, argv, false);

    X_GUARD(SLC, ServiceIoContext, std::vector{ xNetAddress::Parse("127.0.0.1:10200") });
    SLC.EnableServerIdCenterUpdate(true);
    SLC.EnableServerListSlaveUpdate(true);
    SLC.EnableRelayInfoDispatcherRelayPortUpdate(true);
    SLC.EnableRelayInfoDispatcherObserverPortUpdate(true);
    SLC.EnableServerTestUpdate(true);

    SLC.OnServerListUpdated = [](eServiceType T, xVersion V, const std::vector<xServiceInfo> & List) {
        cout << "on server list updated: type=" << (unsigned)T << ", version=" << V << endl;
        for (const auto & Item : List) {
            cout << "server_id=" << Item.ServerId << ", server_address=" << Item.Address.ToString() << endl;
        }
        cout << "end of server list" << endl;
    };

    while (ServiceRunState) {
        ServiceUpdateOnce(SLC);
    }

    return 0;
}
