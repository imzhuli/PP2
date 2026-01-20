#include "./server_id_manager.hpp"

#include <object/object.hpp>
#include <pp_common/_.hpp>
#include <pp_common/service_runtime.hpp>
#include <pp_protocol/command.hpp>
#include <pp_protocol/internal/server_id.hpp>

static constexpr const size_t MAX_CONNECTION = 20'0000;

static xServerIdManager ServerIdManager;
static xNetAddress      BindAddress4;
static xTcpService      TcpService;

static bool OnClientPacket(const xTcpServiceClientConnectionHandle & Handle, xPacketCommandId CommandId, xPacketRequestId RequestId, ubyte * PayloadPtr, size_t PayloadSize) {
    if (CommandId != Cmd_AcquireServerId) {
        Logger->E("Unrecognized command");
        return false;
    }
    auto & ServerId = Handle->UserContext.U64;
    if (ServerId) {
        Logger->E("Multiple server id request");
        return false;
    }
    auto Req = xPP_AcquireServerId();
    if (!Req.Deserialize(PayloadPtr, PayloadSize)) {
        Logger->E("Invalid data packet");
        return false;
    }

    ServerId = ServerIdManager.RegainServerId(Req.PreviousServerId);
    Logger->I("ServerId %" PRIu64 " -> %" PRIu64 "", Req.PreviousServerId, ServerId);

    auto Resp             = xPP_AcquireServerIdResp();
    Resp.PreviousServerId = Req.PreviousServerId;
    Resp.NewServerId      = ServerId;
    Handle.PostMessage(Cmd_AcquireServerIdResp, RequestId, Resp);
    return true;
}

static void OnClientClean(const xTcpServiceClientConnectionHandle & Handle) {
    auto & ServerId = Handle->UserContext.U64;
    if (ServerId) {
        Logger->I("Releasing ServerId: %" PRIu64 "", ServerId);
        X_RUNTIME_ASSERT(ServerIdManager.ReleaseServerId(ServerId));
    }
}

int main(int argc, char ** argv) {

    auto SEG = xServiceEnvironmentGuard(argc, argv);
    auto CL  = xConfigLoader(ServiceEnvironment.DefaultConfigFilePath);
    CL.Require(BindAddress4, "BindAddress4");

    TcpService.OnClientPacket = OnClientPacket;
    TcpService.OnClientClean  = OnClientClean;
    X_RESOURCE_GUARD(ServerIdManager);

    RuntimeAssert(BindAddress4.Is4() && BindAddress4.Port, "ipv4 support only");
    X_RESOURCE_GUARD(TcpService, ServiceIoContext, BindAddress4, MAX_CONNECTION);

    while (ServiceRunState) {
        ServiceUpdateOnce(TcpService);
    }

    return 0;
}
