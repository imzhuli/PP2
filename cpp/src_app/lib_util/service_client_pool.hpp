#pragma once
#include <pp_common/_.hpp>
#include <server_arch/client_pool.hpp>

class xPPClientPool final {

public:
    bool Init(size_t MaxConnectionCount);
    void Clean();
    void Tick(uint64_t NowMS);

    void UpdateServerList(const std::vector<xServerInfo> & ServerInfoList);
    void PostMessageByConnectionId(uint64_t ConnectionId, xPacketCommandId CmdId, xPacketRequestId RequestId, xBinaryMessage & Message);
    void PostMessageByHash(uint64_t Hash, xPacketCommandId CmdId, xPacketRequestId RequestId, xBinaryMessage & Message);
    void PostMessage(xPacketCommandId CmdId, xPacketRequestId RequestId, xBinaryMessage & Message);

    xClientPool::xOnTargetConnected OnServerReady = Noop<>;
    xClientPool::xOnTargetClean     OnServerClean = Noop<>;
    xClientPool::xOnTargetPacket    OnPacket      = Noop<true>;

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
    bool OnPacketCallback(const xClientPoolConnectionHandle & CC, xPacketCommandId CommandId, xPacketRequestId RequestId, ubyte * PayloadPtr, size_t PayloadSize);
    void OnTargetConnected(const xClientPoolConnectionHandle & CC);
    void OnTargetClose(const xClientPoolConnectionHandle & CC);
    void OnTargetClean(const xClientPoolConnectionHandle & CC);

private:
    xClientPool                                   ClientPool;
    xAutoHolder<std::vector<xInternalServerInfo>> SortedServerList;

    // optimization
    xAutoHolder<std::vector<xServerInfo>>         TmpAdd;
    xAutoHolder<std::vector<xInternalServerInfo>> TmpRem;
};
