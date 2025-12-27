#include "./server_list.hpp"

#include <pp_protocol/command.hpp>
#include <pp_protocol/internal/server_list.hpp>

static constexpr uint64_t MIN_UPDATE_SERVER_LIST_TIMEOUT_MS = 15'000;

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
    Reset(EnabledDownloaders);
    ClientPool.Clean();
}

void xServerListClient::Tick(uint64_t NowMS) {
    ClientPool.Tick(NowMS);

    auto UpdateServerListTimepoint = NowMS - MIN_UPDATE_SERVER_LIST_TIMEOUT_MS;
    if (LastTickTimestampMS > UpdateServerListTimepoint) {
        return;
    }
    LastTickTimestampMS = NowMS;
    TryDownloadServerList(NowMS);
}

//////////

void xServerListClient::OnTargetConnected(const xClientPoolConnectionHandle & CC) {
    TryDownloadServerList(ClientPool.GetTickTimeMS());
}

void xServerListClient::OnTargetClose(const xClientPoolConnectionHandle & CC) {
    Pass();
}

void xServerListClient::OnTargetClean(const xClientPoolConnectionHandle & CC) {
    Pass();
}

bool xServerListClient::OnDownloadServiceListResp(ubyte * PayloadPtr, size_t PayloadSize) {
    auto Response = xPP_DownloadServiceListResp();
    if (!Response.Deserialize(PayloadPtr, PayloadSize)) {
        return false;
    }

    for (auto & D : EnabledDownloaders) {
        if (D.ServiceType == Response.ServiceType) {
            if (D.LastVersion == Response.Version) {
                return true;
            }
            D.LastVersion = Response.Version;
            OnServerListUpdated(Response.ServiceType, Response.Version, std::move(Response.ServiceInfoList));
            return true;
        }
    }
    return true;
}

bool xServerListClient::OnTargetPacket(const xClientPoolConnectionHandle & CC, xPacketCommandId CommandId, xPacketRequestId RequestId, ubyte * PayloadPtr, size_t PayloadSize) {
    switch (CommandId) {
        case Cmd_DownloadServiceListResp:
            return OnDownloadServiceListResp(PayloadPtr, PayloadSize);
        default:
            break;
    }

    return true;
}

bool xServerListClient::RequestServerListByType(eServiceType Type, xVersion CurrentVersion) {
    auto Request        = xPP_DownloadServiceList();
    Request.ServiceType = Type;
    Request.LastVersion = CurrentVersion;
    return ClientPool.PostMessage(Cmd_DownloadServiceList, 0, Request);
}

//////////
void xServerListClient::EnableDownloader(eServiceType ServiceType, uint64_t RequestTimeoutMS) {
    assert(ServiceType != eServiceType::Unspecified);
    auto ND = xDownloader{
        .ServiceType      = ServiceType,
        .RequestTimeoutMS = std::max(RequestTimeoutMS, MIN_UPDATE_SERVER_LIST_TIMEOUT_MS),
    };
    for (auto & D : EnabledDownloaders) {
        if (D.ServiceType == ServiceType) {
            D = ND;
            return;
        }
    }
    EnabledDownloaders.push_back(ND);
}

void xServerListClient::DisableDownloader(eServiceType Type) {
    auto End = EnabledDownloaders.end();
    for (auto I = EnabledDownloaders.begin(); I != End; ++I) {
        if (I->ServiceType == Type) {
            EnabledDownloaders.erase(I);
            return;
        }
    }
}

void xServerListClient::TryDownloader(xDownloader & Downloader, uint64_t NowMS) {
    bool ShouldUpdate = Downloader.LastRequestTimestampMS < NowMS - Downloader.RequestTimeoutMS;
    if (!ShouldUpdate) {
        return;
    }
    if (RequestServerListByType(Downloader.ServiceType, Downloader.LastVersion)) {
        Downloader.LastRequestTimestampMS = NowMS;
    }
}

void xServerListClient::TryDownloadServerList(uint64_t NowMS) {
    for (auto & D : EnabledDownloaders) {
        TryDownloader(D, NowMS);
    }
}
