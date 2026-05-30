#include "./server_id_client.hpp"

#include "./server_id_manager.hpp"

#include <fstream>
#include <pp_common/service_runtime.hpp>
#include <pp_protocol/command.hpp>
#include <pp_protocol/p_register_server.hpp>

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

bool xServerIdClient::Init(const xServerIdClientOptions & Options, const xNetAddress & ServerIdCenterAddress) {
    if (!ClientWrapper.Init(ServiceIoContext)) {
        return false;
    }
    ClientWrapper.OnServerConnected = Delegate(&xServerIdClient::OnServerConnected, this);
    ClientWrapper.OnServerPacket    = Delegate(&xServerIdClient::OnServerPacket, this);

    ServerType    = Options.ServerType;
    ExportAddress = Options.ExportAddress;
    if ((LocalServerId = Options.PreviousServerId)) {
        auto CheckType = ExtractServerType(LocalServerId);
        if (CheckType != ServerType) {
            Logger->I("ServerType dont match, reset LocalServerId to zero");
            Reset(LocalServerId);
        }
    }
    ClientWrapper.UpdateTarget(ServerIdCenterAddress);
    return LocalServerIdDirty = true;
}

bool xServerIdClient::Init(const xServerIdClientOptions & Options, const xNetAddress & ServerIdCenterAddress, const std::string & LocalServerIdFilename) {
    if (!ClientWrapper.Init(ServiceIoContext)) {
        return false;
    }
    ClientWrapper.OnServerConnected = Delegate(&xServerIdClient::OnServerConnected, this);
    ClientWrapper.OnServerPacket    = Delegate(&xServerIdClient::OnServerPacket, this);

    ServerType    = Options.ServerType;
    ExportAddress = Options.ExportAddress;
    if ((LocalServerId = LoadLocalServerId(this->LocalServerIdFilename = LocalServerIdFilename))) {
        auto CheckType = ExtractServerType(LocalServerId);
        if (CheckType != ServerType) {
            Logger->I("ServerType dont match, reset LocalServerId to zero");
            Reset(LocalServerId);
        }
    } else if ((LocalServerId = Options.PreviousServerId)) {
        auto CheckType = ExtractServerType(LocalServerId);
        if (CheckType != ServerType) {
            Logger->I("ServerType dont match, reset LocalServerId to zero");
            Reset(LocalServerId);
        }
    }
    ClientWrapper.UpdateTarget(ServerIdCenterAddress);
    return LocalServerIdDirty = true;
}

void xServerIdClient::Clean() {
    Reset(ServerType);
    Reset(ExportAddress);
    Reset(LocalServerId);
    ClientWrapper.Clean();
    ClientWrapper.UpdateTarget({});
}

void xServerIdClient::Tick(uint64_t NowMS) {
    ClientWrapper.Tick(NowMS);
}

void xServerIdClient::OnServerConnected() {
    auto Req             = xPP_RegisterServer();
    Req.ServerType       = ServerType;
    Req.PreviousServerId = LocalServerId;
    Req.ExportAddress    = ExportAddress;
    ClientWrapper.PostMessage(Cmd_RegisterService, 0, Req);
}

bool xServerIdClient::OnServerPacket(xPacketCommandId CommandId, xPacketRequestId RequestId, ubyte * PayloadPtr, size_t PayloadSize) {
    if (CommandId != Cmd_RegisterServiceResp) {
        DEBUG_LOG("invalid command id");
        return false;
    }

    auto Resp = xPP_RegisterServerResp();
    if (!Resp.Deserialize(PayloadPtr, PayloadSize)) {
        DEBUG_LOG("invalid packet");
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
