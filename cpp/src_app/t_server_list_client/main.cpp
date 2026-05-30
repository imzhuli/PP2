#include <pp_common/service_runtime.hpp>
#include <pp_protocol/command.hpp>
#include <pp_protocol/p_small_server_list.hpp>

static auto ServerListClient = xUdpService();
static auto TargetAddress    = xNetAddress::Parse("127.0.0.1:11000");

static void OnDownloadListResp(const xUdpServiceChannelHandle & Handle, xPacketCommandId CommandId, xPacketRequestId RequestId, ubyte * Payload, size_t PayloadSize) {
    if (CommandId != Cmd_DownloadSmallServerListResp) {
        return;
    }

    auto Resp       = xPP_GetSmallServerListResp();
    auto TempList   = xPP_GetSmallServerListResp::xSmallServerList();
    Resp.ServerList = &TempList;

    if (!Resp.Deserialize(Payload, PayloadSize)) {
        return;
    }
    Logger->D("OnDownloadListResp: total=%" PRIu32 "", Resp.ServerListSize);
    for (size_t I = 0; I < Resp.ServerListSize; ++I) {
        auto & SI = TempList[I];
        Logger->D("ServerId=%" PRIx64 ", ServerExportAddress=%s", SI.ServerId, SI.Address.ToString().c_str());
    }
}

int main(int argc, char ** argv) {
    X_VAR xServiceEnvironmentGuard(argc, argv, ServiceConsoleLogger);

    X_RESOURCE_GUARD_ASSERTED(ServerListClient, ServiceIoContext, xNetAddress::Make4());
    ServerListClient.OnPacket = OnDownloadListResp;

    auto Timer = xel::xTimer();
    while (ServiceRunState) {
        ServiceUpdateOnce();
        if (Timer.TestAndTag(5s)) {
            auto Req       = xPP_GetSmallServerList();
            Req.ServerType = ST_TARGET_COLLECTOR;
            ServerListClient.PostMessage(TargetAddress, Cmd_DownloadSmallServerList, 0, Req);
        }
    }

    return 0;
}
