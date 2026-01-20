#include "./server_id.hpp"

#include <fstream>
#include <pp_protocol/command.hpp>
#include <pp_protocol/internal/server_id.hpp>

uint64_t LoadLocalServerId(const std::string & LocalServerIdFilename) {
    auto File  = LocalServerIdFilename;
    auto FSOpt = FileToStr(File);
    if (!FSOpt) {
        return 0;
    }
    return (uint64_t)strtoumax(FSOpt->c_str(), nullptr, 10);
}

void DumpLocalServerId(const std::string & LocalServerIdFilename, uint64_t LocalServerId) {
    if (LocalServerIdFilename.empty()) {
        return;
    }
    auto File  = LocalServerIdFilename;
    auto FSOpt = std::ofstream(File, std::ios_base::binary | std::ios_base::out);
    if (!FSOpt) {
        cerr << "failed to dump file to LocalCacheFile" << endl;
        return;
    }
    FSOpt << LocalServerId << endl;
    return;
}

//////////////////////////////////////////////////////

bool xServerIdClient::Init(xIoContext * ICP, uint64_t FirstTryServerId) {
    if (!ClientWrapper.Init(ICP)) {
        return false;
    }
    ClientWrapper.OnServerConnected = Delegate(&xServerIdClient::OnServerConnected, this);
    ClientWrapper.OnServerPacket    = Delegate(&xServerIdClient::OnServerPacket, this);

    LocalServerId             = FirstTryServerId;
    return LocalServerIdDirty = true;
}

bool xServerIdClient::Init(xIoContext * ICP, const std::string & LocalServerIdFilename) {
    if (!ClientWrapper.Init(ICP)) {
        return false;
    }
    ClientWrapper.OnServerConnected = Delegate(&xServerIdClient::OnServerConnected, this);
    ClientWrapper.OnServerPacket    = Delegate(&xServerIdClient::OnServerPacket, this);

    this->LocalServerId       = LoadLocalServerId(this->LocalServerIdFilename = LocalServerIdFilename);
    return LocalServerIdDirty = true;
}

void xServerIdClient::Clean() {
    Reset(LocalServerId);
    ClientWrapper.Clean();
}

void xServerIdClient::Tick(uint64_t NowMS) {
    ClientWrapper.Tick(NowMS);
}

void xServerIdClient::OnServerConnected() {
    auto Req             = xPP_AcquireServerId();
    Req.PreviousServerId = LocalServerId;
    ClientWrapper.PostMessage(Cmd_AcquireServerId, 0, Req);
}

bool xServerIdClient::OnServerPacket(xPacketCommandId CommandId, xPacketRequestId RequestId, ubyte * PayloadPtr, size_t PayloadSize) {
    if (CommandId != Cmd_AcquireServerIdResp) {
        return false;
    }

    auto Resp = xPP_AcquireServerIdResp();
    if (!Resp.Deserialize(PayloadPtr, PayloadSize)) {
        X_DEBUG_PRINTF("invalid packet");
        return false;
    }
    if (LocalServerId != Resp.NewServerId) {
        LocalServerId      = Resp.NewServerId;
        LocalServerIdDirty = true;
    }
    if (Steal(LocalServerIdDirty)) {
        if (!LocalServerIdFilename.empty()) {
            DumpLocalServerId(LocalServerIdFilename, LocalServerId);
        }
        OnServerIdUpdated(LocalServerId);
    }
    return true;
}
