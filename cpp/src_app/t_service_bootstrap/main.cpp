#include "../lib_client/server_id.hpp"

#include <pp_common/service_runtime.hpp>

xNetAddress ServerIdAddress                = xNetAddress::Parse("127.0.0.1:10100");
xNetAddress ServerListMasterServiceAddress = xNetAddress::Parse("127.0.0.1:10200");
xNetAddress MyServiceAddress               = xNetAddress::Parse("127.0.0.1:10300");

int main(int argc, char ** argv) {
    X_VAR xServiceRuntimeEnvGuard(argc, argv, false);

    while (ServiceRunState) {
        ServiceUpdateOnce();
    }

    return 0;
}
