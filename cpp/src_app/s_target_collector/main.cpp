#include "../lib_component/server_id_client.hpp"
#include "../lib_component/small_server_list_downloader.hpp"
#include "./reporter.hpp"

#include <pp_common/service_runtime.hpp>

static auto BindAddress           = xNetAddress();
static auto ExportAddress         = xNetAddress();
static auto ServerIdServerAddress = xNetAddress();
static auto CollectorService      = xUdpService();
static auto ServerIdClient        = xServerIdClient();
static auto ServerListDownloader  = xSmallServerListDownloader();

void OnUdpPacket(const xUdpServiceChannelHandle & Handle, xPacketCommandId CmdId, xPacketRequestId RequestId, ubyte * Payload, size_t PayloadSize) {
    DEBUG_LOG("Packet:\n%s", HexShow(Payload, PayloadSize).c_str());
}

int main(int argc, char ** argv) {
    X_VAR xServiceEnvironmentGuard(argc, argv);
    auto  CL = ServiceEnvironment.LoadConfig();
    CL.Require(BindAddress, "BindAddress");
    CL.Require(ExportAddress, "ExportAddress");
    CL.Require(ServerIdServerAddress, "ServerIdServerAddress");

    X_RESOURCE_GUARD_ASSERTED(CollectorService, ServiceIoContext, BindAddress);

    auto ServerIdClientOptions = xServerIdClientOptions{
        .ServerType       = ST_TARGET_COLLECTOR,
        .PreviousServerId = 0,
        .ExportAddress    = ExportAddress,
    };
    auto ServerIdFilename = ServiceEnvironment.DefaultLocalServerIdFilePath;
    X_RESOURCE_GUARD_ASSERTED(ServerIdClient, ServerIdClientOptions, ServerIdServerAddress, ServerIdFilename);

    CollectorService.OnPacket = OnUdpPacket;

    while (ServiceRunState) {
        ServiceUpdateOnce(ServerIdClient);
    };

    return 0;
}
