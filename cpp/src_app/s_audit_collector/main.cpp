#include "../lib_component/server_id_client.hpp"

#include <pp_common/service_runtime.hpp>

static constexpr const size_t MAX_REPORTER_CONNECTION_COUNT = 5'0000;

// config
static auto BindAddress           = xNetAddress();
static auto ExportAddress         = xNetAddress();
static auto ServerIdServerAddress = xNetAddress();

//
static auto ServerIdClient = xServerIdClient();
static auto TcpService     = xTcpService();

int main(int argc, char ** argv) {
    X_VAR xServiceEnvironmentGuard(argc, argv);
    auto  CL = ServiceEnvironment.LoadConfig();

    CL.Require(BindAddress, "BindAddress");
    CL.Require(ExportAddress, "ExportAddress");
    CL.Require(ServerIdServerAddress, "ServerIdServerAddress");

    auto ServerIdClientOptions = xServerIdClientOptions{
        .ServerType       = ST_AUDIT_COLLECTOR,
        .PreviousServerId = 0,
        .ExportAddress    = ExportAddress,
    };
    auto ServerIdFilename = ServiceEnvironment.DefaultLocalServerIdFilePath;
    X_RESOURCE_GUARD_ASSERTED(ServerIdClient, ServerIdClientOptions, ServerIdServerAddress, ServerIdFilename);
    X_RESOURCE_GUARD_ASSERTED(TcpService, ServiceIoContext, BindAddress, MAX_REPORTER_CONNECTION_COUNT);

    while (ServiceRunState) {
        ServiceUpdateOnce(ServerIdClient, TcpService);
    }

    return 0;
}