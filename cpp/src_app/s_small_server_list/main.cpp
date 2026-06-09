#include "../lib_component/server_id_service.hpp"

#include <pp_common/service_runtime.hpp>
#include <pp_protocol/command.hpp>
#include <pp_protocol/p_small_server_list.hpp>

static auto BindAddress     = xNetAddress();
static auto ServerIdService = xServerIdService();

namespace {

    using xSmallServerList = std::array<xServerInfo, MAX_SMALL_SERVER_LIST_SIZE>;
    struct xBufferedSmallListResp {
        ubyte  Buffer[xel::MaxPacketSize];
        size_t DataSize = 0;
    };
    struct xBufferedSmallList {
        xSmallServerList       List;
        xBufferedSmallListResp Resp;
        //
        bool                   Dirty              = true;
        uint64_t               VersionTimestampMS = 0;
    };
    static xBufferedSmallList * ServerListMap[256] = {};  //

    static xUdpService ServerListDownloadService;

}  // namespace

static void EnableServerType(xServerType Type) {
    auto & List = ServerListMap[Type];
    assert(!List);
    List = new xBufferedSmallList();
}

static void DisableServerType(xServerType Type) {
    auto List = Steal(ServerListMap[Type]);
    assert(List);
    delete List;
}

static void BuildServerListResponse(xBufferedSmallList * List, xPacketRequestId RequestId) {
    if (Steal(List->Dirty)) {
        auto NewVersionTimestampMS = xel::GetTimestampMS();
        if (NewVersionTimestampMS <= List->VersionTimestampMS) {
            NewVersionTimestampMS = List->VersionTimestampMS + 1;
        }
        List->VersionTimestampMS = NewVersionTimestampMS;

        auto Total    = 0;
        auto TempList = xSmallServerList();
        for (size_t I = 0; I < MAX_SMALL_SERVER_LIST_SIZE; ++I) {
            auto & Ref = List->List[I];
            if (!Ref.ServerId) {
                continue;
            }
            TempList[Total] = Ref;
            DEBUG_LOG("Push back serverInfo @%zi, ServerId=%" PRIx64 "", Total, Ref.ServerId);
            ++Total;
        }
        auto Resp               = xPP_GetSmallServerListResp();
        Resp.VersionTimestampMS = List->VersionTimestampMS;
        Resp.ServerList         = &TempList;
        Resp.ServerListSize     = Total;
        List->Resp.DataSize     = xel::WriteMessage(List->Resp.Buffer, Cmd_DownloadSmallServerListResp, 0, Resp);

        DEBUG_LOG("update resp buffer: \n%s", HexShow(List->Resp.Buffer, List->Resp.DataSize).c_str());
    }
    // in-situ rebuild header
    xel::xPacketHeader::PatchRequestId(List->Resp.Buffer, RequestId);
}

static void OnNewServerId(xServerId ServerId, const xNetAddress & ExportAddress) {
    DEBUG_LOG("ServerId=%" PRIx64 ", Address=%s", ServerId, ExportAddress.ToString().c_str());
    auto [Type, ObjectId] = ExtractServerIdInternal(ServerId);
    auto List             = ServerListMap[Type];
    X_PASS_ASSERT(List);
    X_PASS_ASSERT(ObjectId);
    auto Index = ObjectId - 1;
    if (Index >= MAX_SMALL_SERVER_LIST_SIZE) {
        Logger->E("ServerId exceeds  MAX_SMALL_SERVER_LIST_SIZE");
        return;
    }
    auto & SI = List->List[Index];
    X_PASS_ASSERT(!SI.ServerId && !SI.Address);

    Reset(SI, xServerInfo{ .ServerId = ServerId, .Address = ExportAddress });
    List->Dirty = true;
}

static void OnRemoveServerId(xServerId ServerId) {
    DEBUG_LOG("ServerId=%" PRIx64 "", ServerId);
    auto [Type, ObjectId] = ExtractServerIdInternal(ServerId);
    auto List             = ServerListMap[Type];
    X_PASS_ASSERT(List);
    X_PASS_ASSERT(ObjectId);
    auto Index = ObjectId - 1;
    if (Index >= MAX_SMALL_SERVER_LIST_SIZE) {
        Logger->E("ServerId exceeds  MAX_SMALL_SERVER_LIST_SIZE");
        return;
    }
    auto & SI = List->List[Index];
    Reset(SI);
    List->Dirty = true;
}

static void OnDownloadList(const xUdpServiceChannelHandle & Handle, xPacketCommandId CommandId, xPacketRequestId RequestId, ubyte * Payload, size_t PayloadSize) {
    if (CommandId != Cmd_DownloadSmallServerList) {
        return;
    }
    auto Req = xPP_GetSmallServerList();
    if (!Req.Deserialize(Payload, PayloadSize)) {
        DEBUG_LOG("invalid protocol");
        return;
    }
    auto List = ServerListMap[Req.ServerType];
    if (!List) {
        DEBUG_LOG("disabled server type");
        return;
    }
    BuildServerListResponse(List, RequestId);
    Handle.PostData(List->Resp.Buffer, List->Resp.DataSize);
}

int main(int argc, char ** argv) {

    X_VAR xServiceEnvironmentGuard(argc, argv);
    auto  CL = ServiceEnvironment.LoadConfig();
    CL.Require(BindAddress, "BindAddress");

    X_RESOURCE_GUARD_ASSERTED(ServerIdService, ServiceIoContext, BindAddress);
    X_SCOPE_GUARD(
        [] {
            EnableServerType(ST_SERVER_LIST);
            EnableServerType(ST_TARGET_COLLECTOR);
            EnableServerType(ST_AUDIT_COLLECTOR);
            ServerIdService.EnableServerType(ST_SERVER_LIST);
            ServerIdService.EnableServerType(ST_TARGET_COLLECTOR);
            ServerIdService.EnableServerType(ST_AUDIT_COLLECTOR);
        },
        [] {
            DisableServerType(ST_SERVER_LIST);
            DisableServerType(ST_TARGET_COLLECTOR);
            DisableServerType(ST_AUDIT_COLLECTOR);
            ServerIdService.DisableServerType(ST_SERVER_LIST);
            ServerIdService.DisableServerType(ST_TARGET_COLLECTOR);
            ServerIdService.DisableServerType(ST_AUDIT_COLLECTOR);
        }
    );
    ServerIdService.OnNewServerId    = OnNewServerId;
    ServerIdService.OnRemoveServerId = OnRemoveServerId;

    X_RESOURCE_GUARD_ASSERTED(ServerListDownloadService, ServiceIoContext, BindAddress);
    ServerListDownloadService.OnPacket = OnDownloadList;

    while (ServiceRunState) {
        ServiceUpdateOnce(ServerIdService);
    }

    return 0;
}
