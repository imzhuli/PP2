#include "./server_id_service.hpp"

#include <pp_common/service_runtime.hpp>
#include <pp_protocol/command.hpp>
#include <pp_protocol/p_register_server.hpp>

static constexpr const uint64_t INIT_REGISTER_SERVER_ID_TIMEOUT = 3'000;
static constexpr const size_t   MAX_SERVER_CONNECTIONS          = 100'000;

bool xServerIdService::Init(const xel::xNetAddress & BindAddress) {
    assert(!IdManagerList);
    IdManagerList = new xIdManagerList{};

    if (!ConnectionContextPool.Init({
            .InitSize    = MAX_SERVER_CONNECTIONS,
            .MaxPoolSize = MAX_SERVER_CONNECTIONS,
        })) {
        delete Steal(IdManagerList);
        return false;
    }

    if (!TcpService.Init(ServiceIoContext, BindAddress, MAX_SERVER_CONNECTIONS)) {
        ConnectionContextPool.Clean();
        delete Steal(IdManagerList);
        return false;
    }

    TcpService.OnClientConnected = Delegate(&xServerIdService::OnNewClientConnection, this);
    TcpService.OnClientPacket    = Delegate(&xServerIdService::OnClientConnectionPacket, this);
    TcpService.OnClientClean     = Delegate(&xServerIdService::OnCleanClientConnection, this);

    return true;
}

void xServerIdService::Clean() {
    TcpService.Clean();
    ConnectionContextPool.Clean();
    for (auto & IdManager : *IdManagerList) {
        if (IdManager) {
            IdManager->Clean();
        }
    }
    delete Steal(IdManagerList);
}

bool xServerIdService::EnableServerType(xServerType Type) {
    auto & IdManager = (*IdManagerList)[Type];
    assert(!IdManager);
    IdManager = new xServerIdManager();
    if (!IdManager->Init()) {
        delete Steal(IdManager);
        return false;
    }
    return true;
}

void xServerIdService::DisableServerType(xServerType Type) {
    auto & IdManager = (*IdManagerList)[Type];
    assert(IdManager);
    IdManager->Clean();
    delete Steal(IdManager);
}

//////////

void xServerIdService::Tick(uint64_t NowMS) {
    LocalTicker.Update(NowMS);
    KillUnregisteredClientConnections();
    TcpService.Tick(NowMS);
}

xServerIdManager * xServerIdService::GetServerIdManagerByServerId(uint64_t ServerId) {
    auto Type = ExtractServerType(ServerId);
    return (*IdManagerList)[Type];
}

void xServerIdService::KillUnregisteredClientConnections() {
    auto Cond = [KillTimepointMS = LocalTicker() - INIT_REGISTER_SERVER_ID_TIMEOUT](const xConnectionContext & C) {
        return C.StartupTimestampMS <= KillTimepointMS;
    };
    while (auto Context = TimeoutContextList.PopHead(Cond)) {
        assert(!Context->ServerId);
        assert(Context->ConnectionHandle.IsValid());
        Context->ConnectionHandle.Kill();
    }
}

void xServerIdService::OnNewClientConnection(const xTcpServiceClientConnectionHandle & Handle) {
    DEBUG_LOG("ConnectionId=%" PRIx64 "", Handle.GetConnectionId());
    auto Context = ConnectionContextPool.Create();
    if (!Context) {
        Handle.Kill();
        return;
    }
    Context->StartupTimestampMS = LocalTicker();
    Context->ConnectionHandle   = Handle;
    TimeoutContextList.AddTail(*Context);

    Handle->UserContext.P = Context;
}

void xServerIdService::OnCleanClientConnection(const xTcpServiceClientConnectionHandle & Handle) {
    auto Context = static_cast<xConnectionContext *>(Handle->UserContext.P);
    if (!Context) {
        return;
    }
    DEBUG_LOG("ConnectionId=%" PRIx64 ", ServerId=%" PRIx64 "", Handle.GetConnectionId(), Context->ServerId);
    if (auto ServerId = Context->ServerId) {
        X_PASS_ASSERT(GetServerIdManagerByServerId(ServerId)->ReleaseServerId(ServerId));
        OnRemoveServerId(ServerId);
    }
    ConnectionContextPool.Destroy(Context);
}

bool xServerIdService::OnClientConnectionPacket(const xTcpServiceClientConnectionHandle & Handle, xPacketCommandId CommandId, xPacketRequestId, ubyte * Payload, size_t PayloadSize) {
    DEBUG_LOG("ConnectionId=%" PRIx64 "", Handle.GetConnectionId());
    if (CommandId != Cmd_RegisterService) {
        DEBUG_LOG("Invalid command id");
        return false;
    }
    auto Context = static_cast<xConnectionContext *>(Handle->UserContext.P);
    if (Context->ServerId) {  // multiple register
        DEBUG_LOG("multiple registration");
        return false;
    }

    auto Req = xPP_RegisterServer();
    if (!Req.Deserialize(Payload, PayloadSize)) {
        DEBUG_LOG("invalid protocol");
        return false;
    }

    if (Req.PreviousServerId) {
        auto OldType = ExtractServerType(Req.PreviousServerId);
        if (OldType != Req.ServerType) {
            DEBUG_LOG("server type conflict with old server id");
            return false;
        }
    }

    auto IdManager   = (*IdManagerList)[Req.ServerType];
    auto NewServerId = uint64_t(0);
    if (IdManager) {
        if (Req.PreviousServerId) {
            DEBUG_LOG("Try regain old serverId: %" PRIx64 "", Req.PreviousServerId);
            NewServerId = IdManager->RegainServerId(Req.PreviousServerId);
        }
        if (!NewServerId) {
            DEBUG_LOG("Try acquire new serverId");
            NewServerId = IdManager->AcquireServerId(Req.ServerType);
        }
    } else {
        DEBUG_LOG("NoIdManager found");
    }
    if ((Context->ServerId = NewServerId)) {
        xConnectionContextList::Remove(*Context);
        OnNewServerId(NewServerId, Req.ExportAddress);
    }
    DEBUG_LOG("ConnectionId=%" PRIx64 ", ServerId=%" PRIx64 "", Handle.GetConnectionId(), Context->ServerId);

    auto Resp        = xPP_RegisterServerResp();
    Resp.NewServerId = NewServerId;
    Handle.PostMessage(Cmd_RegisterServiceResp, 0, Resp);

    return true;
}
