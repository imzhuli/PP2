#include "../lib_client/server_list.hpp"
#include "../lib_client/server_register.hpp"

#include <pp_common/service_runtime.hpp>

xServerListRegister SLR;

int main(int argc, char ** argv) {
    X_VAR xServiceRuntimeEnvGuard(argc, argv, false);

    X_GUARD(SLR, ServiceIoContext);
    SLR.UpdateMasterServierListAddress(xNetAddress::Parse("127.0.0.1:10200"));
    SLR.UpdateMyServiceInfo(eServiceType::ServerListSlave, xServiceInfo{ .ServerId = (uint64_t)time(nullptr), .Address = xNetAddress::Parse("127.0.0.1:10300") });

    while (ServiceRunState) {
        ServiceUpdateOnce(SLR);
    }

    return 0;
}
