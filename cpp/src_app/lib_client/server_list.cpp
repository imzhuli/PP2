#include "./server_list.hpp"

#include <pp_protocol/command.hpp>
#include <pp_protocol/internal/server_list.hpp>

static uint64_t MIN_UPDATE_SERVER_LIST_TICKER_TIMEOUT_MS = 15'000;

static uint64_t MIN_UPDATE_SERVER_ID_CENTER_TIMEOUT_MS = 10 * 60'000;
static uint64_t MIN_UPDATE_SERVER_LIST_TIMEOUT_MS      = 10 * 60'000;

bool xServerListClient::Init(xIoContext * ICP, const std::vector<xNetAddress> & InitAddresses) {
    auto ListCopy = InitAddresses;
    std::sort(ListCopy.begin(), ListCopy.end());
    auto Last = std::unique(ListCopy.begin(), ListCopy.end());
    ListCopy.erase(Last, ListCopy.end());
    if (ListCopy.empty()) {
        return false;
    }

    if (!ClientPool.Init(ICP, 2000)) {
        return false;
    }

    for (auto & A : ListCopy) {
        ClientPool.AddServer(A);
    }

    ClientPool.OnTargetConnected = Delegate(&xServerListClient::OnTargetConnected, this);
    ClientPool.OnTargetClose     = Delegate(&xServerListClient::OnTargetClose, this);
    ClientPool.OnTargetClean     = Delegate(&xServerListClient::OnTargetClean, this);
    ClientPool.OnTargetPacket    = Delegate(&xServerListClient::OnTargetPacket, this);

    return true;
}

void xServerListClient::Clean() {
    ClientPool.Clean();
}

void xServerListClient::Tick(uint64_t NowMS) {
    ClientPool.Tick(NowMS);

    bool UpdateServerList = NowMS - MIN_UPDATE_SERVER_LIST_TICKER_TIMEOUT_MS;
    if (LastTickTimestampMS > UpdateServerList) {
        return;
    }
    LastTickTimestampMS = NowMS;

    // request server list:
    if (EnableServerIdCenterDownload) {
        bool ShouldUpdate = ServerIdCenterRequestTimestampMS < NowMS - MIN_UPDATE_SERVER_ID_CENTER_TIMEOUT_MS;
        if (!ShouldUpdate) {
            return;
        }
        ServerIdCenterRequestTimestampMS = NowMS;
        RequestServerId();
    }

    if (EnableServerListDownload) {
        bool ShouldUpdate = ServerListRequestTimestampMS < NowMS - MIN_UPDATE_SERVER_LIST_TIMEOUT_MS;
        if (!ShouldUpdate) {
            return;
        }
        ServerListRequestTimestampMS = NowMS;
        RequestServerList();
    }
}

//////////

void xServerListClient::OnTargetConnected(xClientConnection & CC) {
    auto NowMS = ClientPool.GetTickTimeMS();

    if (EnableServerIdCenterDownload) {
        ServerIdCenterRequestTimestampMS = NowMS;
        RequestServerId();
    }

    if (EnableServerListDownload) {
        ServerListRequestTimestampMS = NowMS;
        RequestServerList();
    }
}

void xServerListClient::OnTargetClose(xClientConnection & CC) {
    X_DEBUG_PRINTF();
}

void xServerListClient::OnTargetClean(xClientConnection & CC) {
    X_DEBUG_PRINTF();
}

bool xServerListClient::OnDownloadServiceListResp(ubyte * PayloadPtr, size_t PayloadSize) {
    auto Response = xPP_DownloadServiceListResp();
    if (!Response.Deserialize(PayloadPtr, PayloadSize)) {
        return false;
    }
    OnServerListUpdated(Response.ServiceType, Response.Version, Response.ServiceInfoList);
    return true;
}

bool xServerListClient::OnTargetPacket(xClientConnection & CC, xPacketCommandId CommandId, xPacketRequestId RequestId, ubyte * PayloadPtr, size_t PayloadSize) {
    switch (CommandId) {
        case Cmd_DownloadServiceListResp:
            return OnDownloadServiceListResp(PayloadPtr, PayloadSize);

        default:
            break;
    }

    return true;
}

void xServerListClient::RequestServerListByType(eServiceType Type) {
    auto Request        = xPP_DownloadServiceList();
    Request.ServiceType = Type;
    Request.LastVersion = ServerListVersion;
    ClientPool.PostMessage(Cmd_DownloadServiceList, 0, Request);
}

void xServerListClient::RequestServerId() {
    RequestServerListByType(eServiceType::ServerIdCenter);
}

void xServerListClient::RequestServerList() {
    RequestServerListByType(eServiceType::ServerList);
}
