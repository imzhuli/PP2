#include "./server_list.hpp"

#include <pp_protocol/command.hpp>
#include <pp_protocol/internal/server_list.hpp>

static uint64_t MIN_UPDATE_SERVER_LIST_TICKER_TIMEOUT_MS = 15'000;

static uint64_t MIN_UPDATE_SERVER_ID_CENTER_TIMEOUT_MS                    = 60 * 60'000;
static uint64_t MIN_UPDATE_SERVER_LIST_SLAVE_TIMEOUT_MS                   = 60 * 60'000;
static uint64_t MIN_UPDATE_RELAY_INFO_DISPATCHER_RELAY_PORT_TIMEOUT_MS    = 10 * 60'000;
static uint64_t MIN_UPDATE_RELAY_INFO_DISPATCHER_OBSERVER_PORT_TIMEOUT_MS = 10 * 60'000;
static uint64_t MIN_UPDATE_SERVER_TEST_SLAVE_TIMEOUT_MS                   = 1 * 60'000;

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

    bool UpdateServerListTimepoint = NowMS - MIN_UPDATE_SERVER_LIST_TICKER_TIMEOUT_MS;
    if (LastTickTimestampMS > UpdateServerListTimepoint) {
        return;
    }
    LastTickTimestampMS = NowMS;

    // request server list:
    TryRequestServerId();
    TryRequestServerListSlave();
    TryRequestRelayInfoDispatcherRelayPort();
    TryRequestRelayInfoDispatcherObserverPort();
    //
    TryRequestServerTest();
}

//////////

void xServerListClient::OnTargetConnected(xClientConnection & CC) {
    TryRequestServerId();
    TryRequestServerListSlave();
    TryRequestRelayInfoDispatcherRelayPort();
    TryRequestRelayInfoDispatcherObserverPort();
    //
    TryRequestServerTest();
}

void xServerListClient::OnTargetClose(xClientConnection & CC) {
    X_DEBUG_PRINTF();
}

void xServerListClient::OnTargetClean(xClientConnection & CC) {
    X_DEBUG_PRINTF();
}

bool xServerListClient::OnDownloadServiceListResp(ubyte * PayloadPtr, size_t PayloadSize) {

    X_DEBUG_PRINTF();

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

bool xServerListClient::RequestServerListByType(eServiceType Type) {
    auto Request        = xPP_DownloadServiceList();
    Request.ServiceType = Type;
    Request.LastVersion = ServerListSlaveVersion;
    return ClientPool.PostMessage(Cmd_DownloadServiceList, 0, Request);
}

void xServerListClient::TryRequestServerId() {
    if (!EnableServerIdCenterDownload) {
        return;
    }
    auto NowMS        = ClientPool.GetTickTimeMS();
    bool ShouldUpdate = ServerIdCenterRequestTimestampMS < NowMS - MIN_UPDATE_SERVER_ID_CENTER_TIMEOUT_MS;
    if (!ShouldUpdate) {
        return;
    }
    if (RequestServerListByType(eServiceType::ServerIdCenter)) {
        ServerIdCenterRequestTimestampMS = NowMS;
    }
}

void xServerListClient::TryRequestServerListSlave() {
    if (!EnableServerListSlaveDownload) {
        return;
    }
    auto NowMS        = ClientPool.GetTickTimeMS();
    bool ShouldUpdate = ServerListSlaveRequestTimestampMS < NowMS - MIN_UPDATE_SERVER_LIST_SLAVE_TIMEOUT_MS;
    if (!ShouldUpdate) {
        return;
    }
    if (RequestServerListByType(eServiceType::ServerListSlave)) {
        ServerListSlaveRequestTimestampMS = NowMS;
    }
}

void xServerListClient::TryRequestRelayInfoDispatcherRelayPort() {
    if (!EnableRelayInfoDispatcherRelayPortDownload) {
        return;
    }
    auto NowMS        = ClientPool.GetTickTimeMS();
    bool ShouldUpdate = RelayInfoDispatcherRelayPortRequestTimestampMS < NowMS - MIN_UPDATE_RELAY_INFO_DISPATCHER_RELAY_PORT_TIMEOUT_MS;
    if (!ShouldUpdate) {
        return;
    }
    if (RequestServerListByType(eServiceType::RelayInfoDispatcher_RelayPort)) {
        RelayInfoDispatcherRelayPortRequestTimestampMS = NowMS;
    }
}

void xServerListClient::TryRequestRelayInfoDispatcherObserverPort() {
    if (!EnableRelayInfoDispatcherObserverPortDownload) {
        return;
    }
    auto NowMS        = ClientPool.GetTickTimeMS();
    bool ShouldUpdate = RelayInfoDispatcherObserverPortRequestTimestampMS < NowMS - MIN_UPDATE_RELAY_INFO_DISPATCHER_OBSERVER_PORT_TIMEOUT_MS;
    if (!ShouldUpdate) {
        return;
    }
    if (RequestServerListByType(eServiceType::RelayInfoDispatcher_ObserverPort)) {
        RelayInfoDispatcherObserverPortRequestTimestampMS = NowMS;
    }
}

//////////
void xServerListClient::TryRequestServerTest() {
    if (!EnableServerTestDownload) {
        return;
    }
    auto NowMS        = ClientPool.GetTickTimeMS();
    bool ShouldUpdate = ServerTestRequestTimestampMS < NowMS - MIN_UPDATE_SERVER_TEST_SLAVE_TIMEOUT_MS;
    if (!ShouldUpdate) {
        return;
    }
    if (RequestServerListByType(eServiceType::ServerTest)) {
        ServerTestRequestTimestampMS = NowMS;
    }
}
