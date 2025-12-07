#include <pp_common/_.hpp>
#include <pp_common/_common.hpp>

class xServerListClient {
public:
    bool Init(xIoContext * ICP, const std::vector<xNetAddress> & InitAddresses);
    void Clean();
    void Tick(uint64_t);

    void EnableServerIdCenterUpdate(bool E) {
        EnableServerIdCenterDownload     = E;
        ServerIdCenterRequestTimestampMS = 0;
        ServerIdCenterVersion            = 0;
    }

    void EnableServerListSlaveUpdate(bool E) {
        EnableServerListSlaveDownload     = E;
        ServerListSlaveRequestTimestampMS = 0;
        ServerListSlaveVersion            = 0;
    }

    void EnableRelayInfoDispatcherRelayPortUpdate(bool E) {
        EnableRelayInfoDispatcherRelayPortDownload     = E;
        RelayInfoDispatcherRelayPortRequestTimestampMS = 0;
        RelayInfoDispatcherRelayPortVersion            = 0;
    }

    void EnableRelayInfoDispatcherObserverPortUpdate(bool E) {
        EnableRelayInfoDispatcherObserverPortDownload     = E;
        RelayInfoDispatcherObserverPortRequestTimestampMS = 0;
        RelayInfoDispatcherObserverPortVersion            = 0;
    }

    void EnableServerTestUpdate(bool E) {
        EnableServerTestDownload     = E;
        ServerTestRequestTimestampMS = 0;
        ServerTestVersion            = 0;
    }

    using xOnServerListUpdated = std::function<void(eServiceType, xVersion, const std::vector<xServiceInfo> &)>;

    xOnServerListUpdated OnServerListUpdated = Noop<>;

private:
    void OnTargetConnected(xClientConnection & CC);
    void OnTargetClose(xClientConnection & CC);
    void OnTargetClean(xClientConnection & CC);
    bool OnTargetPacket(xClientConnection & CC, xPacketCommandId CommandId, xPacketRequestId RequestId, ubyte * PayloadPtr, size_t PayloadSize);

    bool RequestServerListByType(eServiceType Type);
    bool OnDownloadServiceListResp(ubyte * PayloadPtr, size_t PayloadSize);

private:
    xel::xClientPool ClientPool;
    uint64_t         LastTickTimestampMS = 0;

    bool     EnableServerIdCenterDownload     = false;
    uint64_t ServerIdCenterRequestTimestampMS = 0;
    xVersion ServerIdCenterVersion            = 0;
    void     TryRequestServerId();

    bool     EnableServerListSlaveDownload     = false;
    uint64_t ServerListSlaveRequestTimestampMS = 0;
    xVersion ServerListSlaveVersion            = 0;
    void     TryRequestServerListSlave();

    bool     EnableRelayInfoDispatcherRelayPortDownload     = false;
    uint64_t RelayInfoDispatcherRelayPortRequestTimestampMS = 0;
    xVersion RelayInfoDispatcherRelayPortVersion            = 0;
    void     TryRequestRelayInfoDispatcherRelayPort();

    bool     EnableRelayInfoDispatcherObserverPortDownload     = false;
    uint64_t RelayInfoDispatcherObserverPortRequestTimestampMS = 0;
    xVersion RelayInfoDispatcherObserverPortVersion            = 0;
    void     TryRequestRelayInfoDispatcherObserverPort();

    /////
    bool     EnableServerTestDownload     = false;
    uint64_t ServerTestRequestTimestampMS = 0;
    xVersion ServerTestVersion            = 0;
    void     TryRequestServerTest();
};
