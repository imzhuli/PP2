#include "../lib_client/server_id.hpp"

#include <pp_common/lang/dispatcher.hpp>
#include <pp_common/service_runtime.hpp>

xNetAddress ServerIdAddress                = xNetAddress::Parse("127.0.0.1:10100");
xNetAddress ServerListMasterServiceAddress = xNetAddress::Parse("127.0.0.1:10200");
xNetAddress MyServiceAddress               = xNetAddress::Parse("127.0.0.1:10300");

auto ServerIdUpdateDispatcher = xDispatcher<uint64_t>();
void OnServerIdUpdated(uint64_t SI) {
    cout << "ServerId updated: " << SI << endl;
}

int main(int argc, char ** argv) {
    X_VAR xServiceRuntimeEnvGuard(argc, argv, false);

    xServerIdClient SIC;
    X_GUARD(SIC, ServiceIoContext);
    SIC.SetServerAddress(ServerIdAddress);
    SIC.OnServerIdUpdated     = ServerIdUpdateDispatcher.Function();
    ServerIdUpdateDispatcher += OnServerIdUpdated;

    while (ServiceRunState) {
        ServiceUpdateOnce(SIC);
    }

    return 0;
}
