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

    void EnableServerListUpdate(bool E) {
        EnableServerListDownload     = E;
        ServerListRequestTimestampMS = 0;
        ServerListVersion            = 0;
    }

    using xOnServerListUpdated = std::function<void(eServiceType, xVersion, const std::vector<xServiceInfo> &)>;

    xOnServerListUpdated OnServerListUpdated = Noop<>;

private:
    void OnTargetConnected(xClientConnection & CC);
    void OnTargetClose(xClientConnection & CC);
    void OnTargetClean(xClientConnection & CC);
    bool OnTargetPacket(xClientConnection & CC, xPacketCommandId CommandId, xPacketRequestId RequestId, ubyte * PayloadPtr, size_t PayloadSize);

    void RequestServerListByType(eServiceType Type);
    bool OnDownloadServiceListResp(ubyte * PayloadPtr, size_t PayloadSize);

private:
    xel::xClientPool ClientPool;
    uint64_t         LastTickTimestampMS = 0;

    bool     EnableServerIdCenterDownload     = false;
    uint64_t ServerIdCenterRequestTimestampMS = 0;
    xVersion ServerIdCenterVersion            = 0;
    void     RequestServerId();

    bool     EnableServerListDownload     = false;
    uint64_t ServerListRequestTimestampMS = 0;
    xVersion ServerListVersion            = 0;
    void     RequestServerList();
};
