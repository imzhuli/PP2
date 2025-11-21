#include "../lib_client/server_id.hpp"
#include "./mindef.hpp"

#include <map>
#include <pp_common/_common.hpp>
#include <pp_common/service_runtime.hpp>
#include <pp_protocol/command.hpp>
#include <pp_protocol/internal/server_id.hpp>
#include <pp_protocol/internal/server_list.hpp>

static xNetAddress     ServerIdCenterAddress;
static xServerIdClient ServerIdClient;

static xTcpService TcpService;
static xNetAddress BindAddress4;
static xNetAddress ExportBindAddress4;
static xNetAddress MasterAddress4;

static bool               IsMaster      = false;
static uint64_t           LocalServerId = 0;
static xServiceInfoByType ServerInfoListArray[(size_t)eServiceType::MAX_TYPE_INDEX];

struct xClientConnectionContext {
    /* if connection is from service*/
    eServiceType ServiceType    = eServiceType::Unspecified;
    uint64_t     ServiceId      = 0;
    xNetAddress  ServiceAddress = {};

    /* */
};

static void IncreaseServiceTypeVersion(xServiceInfoByType & Type) {
    if (!Steal(Type.Dirty, true)) {
        DEBUG_LOG("old value: %u", (unsigned)Type.Version);
        if (!++Type.Version) {
            ++Type.Version;
        }
        DEBUG_LOG("new value: %u", (unsigned)Type.Version);
    }
}

bool InsertServiceInfo(eServiceType ServiceType, uint64_t ServerId, const xNetAddress & ServiceAddress) {
    if (ServiceType >= eServiceType::MAX_TYPE_INDEX) {
        Logger->E("Invalid service type: value overflow");
        return false;
    }
    if (!ServerId && ServiceType != eServiceType::ServerIdCenter) {
        Logger->E("Invalid service id: 0");
        return false;
    }
    auto   InsertNode      = xServiceInfo{ ServerId, ServiceAddress };
    auto & ServiceTypeInfo = ServerInfoListArray[(size_t)ServiceType];
    auto & List            = ServiceTypeInfo.List;
    auto   LB              = std::lower_bound(List.begin(), List.end(), InsertNode, xServiceInfo::LessById);
    if (LB != List.end() && LB->ServerId == ServerId) {  // found & refuse
        Logger->E("Found and refuse");
        return false;
    }

    // insert
    List.insert(LB, InsertNode);
    IncreaseServiceTypeVersion(ServiceTypeInfo);
    return true;
}

void RemoveServerinfoById(eServiceType ServiceType, uint64_t ServerId) {
    auto   FindNode        = xServiceInfo{ ServerId, {} };
    auto & ServiceTypeInfo = ServerInfoListArray[(size_t)ServiceType];
    auto & List            = ServiceTypeInfo.List;
    auto   LB              = std::lower_bound(List.begin(), List.end(), FindNode, xServiceInfo::LessById);
    if (LB != List.end() && LB->ServerId == ServerId) {  // found & remove
        List.erase(LB);
        IncreaseServiceTypeVersion(ServiceTypeInfo);
    }
}

void RebuildServerListWithSelf() {
    if (LocalServerId) {  // remove old local server id
        RemoveServerinfoById(eServiceType::ServerList, LocalServerId);
    }
    LocalServerId = ServerIdClient.GetLocalServerId();
    if (LocalServerId) {
        RuntimeAssert(InsertServiceInfo(eServiceType::ServerList, LocalServerId, ExportBindAddress4));
    }
}

void PrintServerInfoListByType(eServiceType ServiceType) {
    if (ServiceType >= eServiceType::MAX_TYPE_INDEX) {
        return;
    }
    auto &             ServiceTypeInfo = ServerInfoListArray[(size_t)ServiceType];
    auto &             List            = ServiceTypeInfo.List;
    std::ostringstream OS;
    OS << "PrintServerInfoListByType" << endl;
    OS << "begin server type: " << (unsigned)ServiceType << ", Version=" << ServiceTypeInfo.Version << endl;
    for (auto & S : List) {
        OS << "ServerId: " << std::hex << S.ServerId << ", address=" << S.Address.ToString() << endl;
    }
    OS << "end server type: " << (unsigned)ServiceType << endl;
    Logger->I("%s", OS.str().c_str());
}

static void PostRegisterServerAccepted(const xTcpServiceClientConnectionHandle & Handle, bool Accepted) {
    auto Post     = xPP_UpdateServiceInfoResp();
    Post.Accepted = Accepted;
    Handle.PostMessage(Cmd_RegisterServiceResp, 0, Post);
}

static void TryRemovePreviousServiceInfo(const xTcpServiceClientConnectionHandle & Handle) {
    auto PreviousInfo = static_cast<xServerListConnectionContext *>(Steal(Handle->UserContext.P));
    if (!PreviousInfo) {
        return;
    }
    DEBUG_LOG("Type=%u, ServerId=%" PRIu64 "", (unsigned)PreviousInfo->Type, PreviousInfo->Info.ServerId);
    RemoveServerinfoById(PreviousInfo->Type, PreviousInfo->Info.ServerId);
    delete PreviousInfo;
}

static bool OnRegisterServer(const xTcpServiceClientConnectionHandle & Handle, ubyte * PayloadPtr, size_t PayloadSize) {
    auto Request = xPP_UpdateServiceInfo();
    if (!Request.Deserialize(PayloadPtr, PayloadSize)) {
        Logger->E("invalid protocol");
        return false;
    }

    TryRemovePreviousServiceInfo(Handle);
    if (Request.ServiceType == eServiceType::ServerIdCenter) {
        Logger->E("Register server id center is now allowed");
        return false;
    }

    auto Inserted = InsertServiceInfo(Request.ServiceType, Request.ServiceInfo.ServerId, Request.ServiceInfo.Address);
    if (!Inserted) {
        Logger->E(
            "insert new server info failed: Type=%u, ServerId=%" PRIu64 ", Address=%s", (unsigned)Request.ServiceType, Request.ServiceInfo.ServerId,
            Request.ServiceInfo.Address.ToString().c_str()
        );
        return false;
    }
    auto NewInfo = new xServerListConnectionContext{
        .Type = Request.ServiceType,
        .Info = Request.ServiceInfo,
    };
    Handle->UserContext.P = NewInfo;

    Logger->I(
        "ServiceInfo updated: Type=%u, ServerId=%" PRIu64 ", Address=%s", (unsigned)Request.ServiceType, Request.ServiceInfo.ServerId,
        Request.ServiceInfo.Address.ToString().c_str()
    );
    PostRegisterServerAccepted(Handle, true);
    return true;
}

static bool OnDownloadServerList(const xTcpServiceClientConnectionHandle & Handle, ubyte * PayloadPtr, size_t PayloadSize) {
    auto Request = xPP_DownloadServiceList();
    if (!Request.Deserialize(PayloadPtr, PayloadSize)) {
        Logger->E("invalid protocol");
        return false;
    }

    auto & ServiceTypeInfo = ServerInfoListArray[(size_t)Request.ServiceType];
    if (Steal(ServiceTypeInfo.Dirty)) {
        auto Response            = xPP_DownloadServiceListResp();
        Response.ServiceType     = Request.ServiceType;
        Response.Version         = ServiceTypeInfo.Version;
        Response.ServiceInfoList = ServiceTypeInfo.List;

        ServiceTypeInfo.RespSize = WriteMessage(ServiceTypeInfo.RespBuffer, Cmd_DownloadServiceListResp, 0, Response);
        DEBUG_LOG("Updated server list response buffer: \n%s", HexShow(ServiceTypeInfo.RespBuffer, ServiceTypeInfo.RespSize).c_str());
    }
    Handle.PostData(ServiceTypeInfo.RespBuffer, ServiceTypeInfo.RespSize);
    return true;
}

static bool OnClientPacket(const xTcpServiceClientConnectionHandle & Handle, xPacketCommandId CommandId, xPacketRequestId RequestId, ubyte * PayloadPtr, size_t PayloadSize) {
    if (CommandId == Cmd_RegisterService) {
        return OnRegisterServer(Handle, PayloadPtr, PayloadSize);
    }
    if (CommandId == Cmd_DownloadServiceList) {
        return OnDownloadServerList(Handle, PayloadPtr, PayloadSize);
    }
    return true;
}

void OnClientConnected(const xTcpServiceClientConnectionHandle & H) {
    DEBUG_LOG();
}

void OnClientClose(const xTcpServiceClientConnectionHandle & H) {
    DEBUG_LOG();
}

void OnClientClean(const xTcpServiceClientConnectionHandle & H) {
    DEBUG_LOG();
    TryRemovePreviousServiceInfo(H);
}

int main(int argc, char ** argv) {
    X_VAR xServiceRuntimeEnvGuard(argc, argv);

    auto CL = RuntimeEnv.LoadConfig();
    CL.Require(BindAddress4, "BindAddress4");
    CL.Require(ServerIdCenterAddress, "ServerIdCenterAddress");
    CL.Require(ExportBindAddress4, "ExportBindAddress4");
    CL.Optional(MasterAddress4, "MasterAddress4");
    IsMaster = !(MasterAddress4 && MasterAddress4.Port);

    if (!IsMaster) {
        Logger->F("Slave mode not implemented");
        return -1;
    }
    if (MasterAddress4 == ExportBindAddress4) {
        Logger->F("self pointing master server list service");
        return -1;
    }

    Logger->I(
        "===> Starting service, IsMaster=%s, MyExportAddress=(%s), MasterAddress=(%s)", YN(IsMaster), ExportBindAddress4.ToString().c_str(), MasterAddress4.ToString().c_str()
    );

    X_GUARD(TcpService, ServiceIoContext, BindAddress4, 4000);
    TcpService.OnClientConnected = OnClientConnected;
    TcpService.OnClientClose     = OnClientClose;
    TcpService.OnClientClean     = OnClientClean;
    TcpService.OnClientPacket    = OnClientPacket;

    X_GUARD(ServerIdClient, ServiceIoContext, RuntimeEnv.DefaultLocalServerIdFilePath);
    ServerIdClient.SetServerAddress(ServerIdCenterAddress);
    ServerIdClient.OnServerIdUpdated = [](uint64_t UpdatedServerId) {
        Logger->I("Update local server id: %" PRIu64 "", UpdatedServerId);
        RebuildServerListWithSelf();
        PrintServerInfoListByType(eServiceType::ServerList);
    };

    InsertServiceInfo(eServiceType::ServerIdCenter, 0, ServerIdCenterAddress);

    while (ServiceRunState) {
        ServiceUpdateOnce(TcpService, ServerIdClient);
    }

    return 0;
}
