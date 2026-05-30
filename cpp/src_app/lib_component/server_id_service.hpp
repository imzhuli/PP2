#pragma once
#include "./server_id_manager.hpp"

#include <array>
#include <limits>

class xServerIdService {
private:
    using xIdManagerList = std::array<xServerIdManager *, size_t(std::numeric_limits<xServerType>::max()) + 1>;
    struct xConnectionContext : xListNode {
        uint64_t                          StartupTimestampMS = 0;
        uint64_t                          ServerId           = 0;
        xTcpServiceClientConnectionHandle ConnectionHandle   = {};
    };
    using xConnectionContextList = xList<xConnectionContext>;

public:
    bool Init(const xel::xNetAddress & BindAddress);
    void Clean();
    void Tick(uint64_t NowMS);

    bool EnableServerType(xServerType Type);
    void DisableServerType(xServerType Type);

    using xOnNewServerId    = std::function<void(xServerId Id, const xNetAddress & ExportAddress)>;
    using xOnRemoveServerId = std::function<void(xServerId Id)>;

    xOnNewServerId    OnNewServerId    = Noop<>;
    xOnRemoveServerId OnRemoveServerId = Noop<>;

private:
    auto GetServerIdManagerByServerId(uint64_t Serverid) -> xServerIdManager *;
    void KillUnregisteredClientConnections();
    void OnNewClientConnection(const xTcpServiceClientConnectionHandle & Handle);
    void OnCleanClientConnection(const xTcpServiceClientConnectionHandle & Handle);
    bool OnClientConnectionPacket(const xTcpServiceClientConnectionHandle & Handle, xPacketCommandId CommandId, xPacketRequestId, ubyte * Payload, size_t PayloadSize);

private:
    xTicker                         LocalTicker;
    xTcpService                     TcpService;
    xIdManagerList *                IdManagerList = nullptr;
    xMemoryPool<xConnectionContext> ConnectionContextPool;
    xConnectionContextList          TimeoutContextList;
};