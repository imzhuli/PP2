#pragma once
#include <map>
#include <pp_common/_.hpp>

struct xBackendServerInfo {
    xNetAddress Address;
    uint64_t    ConnectionId;
};
inline std::strong_ordering operator<=>(const xBackendServerInfo & lhs, const xBackendServerInfo & rhs) {
    return lhs.Address <=> rhs.Address;
}
inline bool operator==(const xBackendServerInfo & lhs, const xBackendServerInfo & rhs) {
    return lhs.Address == rhs.Address;
}

class xBackendConnectionPool : protected xTcpClientPool {

public:
    bool Init(xIoContext * ICP, size_t MaxConnectionCount);
    void Clean();
    using xTcpClientPool::PostMessage;
    using xTcpClientPool::Tick;

    std::string GetLocalAudit();
    void        ResetLocalAudit();

    using xOnBackendConnectedCallback                      = std::function<void(const xNetAddress & Address)>;
    using xOnBackendPacketCallback                         = std::function<void(xPacketCommandId, xPacketRequestId, ubyte *, size_t)>;
    xOnBackendConnectedCallback OnBackendConnectedCallback = Noop<>;
    xOnBackendPacketCallback    OnBackendPacketCallback    = Noop<>;

public:
    uint64_t AddServer(const xNetAddress & Address, const std::string & AppKey, const std::string & AppSecret);
    void     RemoveServer(const xNetAddress & Address);

private:
    void OnTargetConnectedCallback(const xTcpClientPoolConnectionHandle & CC);
    void OnTargetCloseCallback(const xTcpClientPoolConnectionHandle & CC);
    bool OnTargetPacketCallback(const xTcpClientPoolConnectionHandle & CC, xPacketCommandId CommandId, xPacketRequestId RequestId, ubyte * PayloadPtr, size_t PayloadSize);
    bool OnCmdBackendChallengeResp(const xTcpClientPoolConnectionHandle & CC, xPacketCommandId CommandId, xPacketRequestId RequestId, ubyte * PayloadPtr, size_t PayloadSize);

    //
    struct xBackendConnectionContext {
        std::string AppKey;
        std::string AppSecret;
        bool        IsChallengeReady = false;
    };
    xAutoHolder<std::vector<xBackendConnectionContext>> ContextList;
    xAutoHolder<std::vector<xBackendServerInfo>>        SortedServerList;

private:  // audit
    size_t TotalAddedServer           = 0;
    size_t TotalRemovedServer         = 0;
    size_t TotalAddingServerConflict  = 0;
    size_t TotalRemovingServerFailure = 0;
};
