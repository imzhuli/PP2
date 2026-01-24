#include "./relay_id_client.hpp"

#include <pp_common/service_runtime.hpp>
#include <pp_protocol/command.hpp>
#include <pp_protocol/detail/relay_id.hpp>

bool xRelayIdClient::Init(const xNetAddress & ServerAddress, uint64_t PreviousRelayServerId) {
    AssertServiceEnviromentReady();
    Client.OnServerConnected    = Delegate(&xRelayIdClient::OnServerConnected, this);
    Client.OnServerPacket       = Delegate(&xRelayIdClient::OnServerPacket, this);
    this->PreviousRelayServerId = PreviousRelayServerId;
    return Client.Init(ServiceIoContext, ServerAddress);
}

void xRelayIdClient::Clean() {
    Reset(PreviousRelayServerId);
    Client.Clean();
}

void xRelayIdClient::Tick(uint64_t NowMS) {
    Client.Tick(NowMS);
}

void xRelayIdClient::OnServerConnected() {
    DEBUG_LOG();
    auto Req                  = xAcquireRelayServerId();
    Req.PreviousRelayServerId = PreviousRelayServerId;
    Client.PostMessage(Cmd_AcquireRelayServerId, 0, Req);
}

bool xRelayIdClient::OnServerPacket(xPacketCommandId CommandId, xPacketRequestId RequestId, ubyte * PayloadPtr, size_t PayloadSize) {
    DEBUG_LOG();
    if (CommandId != Cmd_AcquireRelayServerIdResp) {
        AuditLogger->E("invalid command id");
        return false;
    }
    auto Resp = xAcquireRelayServerIdResp();
    if (!Resp.Deserialize(PayloadPtr, PayloadSize)) {
        AuditLogger->E("invalid response");
        return false;
    }

    OnRelayIdUpdated((PreviousRelayServerId = Resp.RelayServerId));
    return true;
}
