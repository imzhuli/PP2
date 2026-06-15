#include "../lib_component/server_id_service.hpp"

#include <pp_common/service_runtime.hpp>
#include <pp_protocol/command.hpp>
#include <pp_protocol/p_small_server_list.hpp>

static auto MasterAddress             = xNetAddress();
static auto BindAddress               = xNetAddress();
static auto ServerListDownloadService = xUdpService();

namespace {

    struct xServerListBuffer {
        ubyte  Buffer[MaxPacketSize];
        size_t DataSize = 0;
    };
    static xServerListBuffer * BufferList[256] = {}

}  // namespace

static void EnableServerGroup(xServerGroup Type) {
    auto & Buffer = BufferList[Type];
    assert(!Buffer);
    List = new xServerListBuffer();
}

static void DisableServerGroup(xServerGroup Type) {
    auto & Buffer = Steal(BufferList[Type]);
    assert(Buffer);
    delete Buffer;
}

static void BuildServerListResponse(xServerListBuffer * Buffer, xPacketRequestId RequestId) {
    xel::xPacketHeader::PatchRequestId(Buffer->Buffer, RequestId);
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
        return;
    }
    auto List = ServerListMap[Req.ServerGroup];
    if (!List) {
        return;
    }
    BuildServerListResponse(List, RequestId);
    Handle.PostData(List->Resp.Buffer, List->Resp.DataSize);
}

int main(int argc, char ** argv) {

    X_VAR xServiceEnvironmentGuard(argc, argv);
    auto  CL = ServiceEnvironment.LoadConfig();
    CL.Require(BindAddress, "BindAddress");

    X_SCOPE_GUARD(
        [] {
            EnableServerGroup(ST_SERVER_LIST);
            EnableServerGroup(ST_TARGET_COLLECTOR);
        },
        [] {
            DisableServerGroup(ST_SERVER_LIST);
            DisableServerGroup(ST_TARGET_COLLECTOR);
        }
    );

    X_RESOURCE_GUARD_ASSERTED(ServerListDownloadService, ServiceIoContext, BindAddress);
    ServerListDownloadService.OnPacket = OnDownloadList;

    while (ServiceRunState) {
        ServiceUpdateOnce(ServerIdService);
    }

    return 0;
}
