#pragma once
#include <pp_common/_.hpp>

class xPPClientPool final {

public:
    bool Init(size_t MaxConnectionCount);
    void Clean();
    void Tick(uint64_t NowMS);

    void UpdateServerList(const std::vector<xServerInfo> & ServerInfoList);
    void PostMessageByConnectionId(uint64_t ConnectionId, xPacketCommandId CmdId, xPacketRequestId RequestId, xBinaryMessage & Message);
    void PostMessageByHash(uint64_t Hash, xPacketCommandId CmdId, xPacketRequestId RequestId, xBinaryMessage & Message);
    void PostMessage(xPacketCommandId CmdId, xPacketRequestId RequestId, xBinaryMessage & Message);

    xTcpClientPool::xOnTargetConnected OnServerReady = Noop<>;
    xTcpClientPool::xOnTargetClean     OnServerClean = Noop<>;
    xTcpClientPool::xOnTargetPacket    OnPacket      = Noop<true>;

private:
    struct xInternalServerInfo final {
        xServerId   ServerId;
        xIndexId    LocalServerId;
        xNetAddress Address;

        bool operator==(const xInternalServerInfo &) const = default;

        static inline bool LessByServerId(const xInternalServerInfo & LHS, const xInternalServerInfo & RHS) { return LHS.ServerId < RHS.ServerId; }
    };

    void AddServer(const xServerInfo & ServerInfo);
    void RemoveServer(const xInternalServerInfo & TargetServerInfo);
    bool OnPacketCallback(const xTcpClientPoolConnectionHandle & CC, xPacketCommandId CommandId, xPacketRequestId RequestId, ubyte * PayloadPtr, size_t PayloadSize);
    void OnTargetConnected(const xTcpClientPoolConnectionHandle & CC);
    void OnTargetClose(const xTcpClientPoolConnectionHandle & CC);
    void OnTargetClean(const xTcpClientPoolConnectionHandle & CC);

private:
    xTcpClientPool                                ClientPool;
    xAutoHolder<std::vector<xInternalServerInfo>> SortedServerList;

    // optimization
    xAutoHolder<std::vector<xServerInfo>>         TmpAdd;
    xAutoHolder<std::vector<xInternalServerInfo>> TmpRem;
};
