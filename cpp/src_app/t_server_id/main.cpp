#include "../lib_component/server_id_client.hpp"
#include "../lib_component/server_id_manager.hpp"

#include <pp_common/service_runtime.hpp>

static auto ServerAddress = xel::xNetAddress::Parse("127.0.0.1:11000");
static auto Client        = xServerIdClient();

int main(int argc, char ** argv) {
    X_VAR xServiceEnvironmentGuard(argc, argv, ServiceConsoleLogger);

    X_RESOURCE_GUARD_ASSERTED(Client, xServerIdClientOptions{ .ServerType = ST_TARGET_COLLECTOR, .PreviousServerId = 0x80080001096567cb, .ExportAddress = xNetAddress::Parse("127.0.0.1:12000") }, ServerAddress);

    while (ServiceRunState) {
        ServiceUpdateOnce(Client);
    }

    return 0;
}
