#include <pp_common/service_runtime.hpp>
#include <pp_protocol/command.hpp>
#include <pp_protocol/detail/relay_id.hpp>

namespace {
    struct xRelayIdContext : xListNode {
        uint64_t OwnerId                    = 0;
        uint64_t ProtectedRelayId           = 0;
        uint64_t ProtectionStartTimestampMS = 0;
    };
}  // namespace

namespace config {
    static xNetAddress BindAddress4;
}

namespace audit {

    static uint64_t InvalidCommandIdCount         = 0;
    static uint64_t InvalidProtocolCount          = 0;
    static uint64_t MultipleCommandCount          = 0;
    static uint64_t InvalidPreviousRelayIdCount   = 0;
    static uint64_t OwnerConflictCount            = 0;
    static uint64_t ProtectedRelayIdConflictCount = 0;

}  // namespace audit

static constexpr const uint64_t RELAY_ID_PROTECT_TIMEOUT = 3 * 60'000;

static xObjectIdManager       RelayIdManager;
static xRelayIdContext        RelayIdContext[xObjectIdManager::MaxObjectId + 1] = {};
static xList<xRelayIdContext> ProtectedRelayIdContextList;
static xTcpService            RelayConnectionService;

static void ProtectRelayServerId(uint64_t Id) {
    DEBUG_LOG("Id=%" PRIx64 "", Id);

    auto Index = Low32(Id);
    assert(Index && Index < RelayIdManager.MaxObjectId);
    auto & Context = RelayIdContext[Index];

    assert(Context.OwnerId);
    assert(Context.ProtectedRelayId == Id);
    assert(!Context.ProtectionStartTimestampMS);
    assert(!xListNode::IsLinked(Context));

    Context.OwnerId                    = 0;
    Context.ProtectionStartTimestampMS = ServiceTicker();
    ProtectedRelayIdContextList.AddTail(Context);
}

static void ActivateProtectedReylayContext(xRelayIdContext & C, uint64_t OwnerId) {
    assert(!C.OwnerId);
    assert(C.ProtectedRelayId);
    assert(C.ProtectionStartTimestampMS);
    assert(xListNode::IsLinked(C));

    DEBUG_LOG("Id=%" PRIx64 ", Owner=%" PRIx64 "", C.ProtectedRelayId, OwnerId);

    C.OwnerId                    = OwnerId;
    C.ProtectionStartTimestampMS = 0;
    ProtectedRelayIdContextList.Remove(C);
}

static uint64_t GenerateRelayServerId(uint64_t OwnerId, uint64_t PreviousRelayId) {
    assert(OwnerId);
    if (PreviousRelayId) {
        auto Index = Low32(PreviousRelayId);
        if (!Index || Index > xObjectIdManager::MaxObjectId) {  // invalid previous relay id
            ++audit::InvalidPreviousRelayIdCount;
            return 0;
        }
        auto & Context = RelayIdContext[Index];
        if (Context.OwnerId) {  // prevent from duplicate connection regain a same server id
            ++audit::OwnerConflictCount;
            goto ALLOC_NEW_ID;
        }
        if (Context.ProtectedRelayId != PreviousRelayId) {
            ++audit::ProtectedRelayIdConflictCount;
            goto ALLOC_NEW_ID;
        }
        ActivateProtectedReylayContext(Context, OwnerId);
        return PreviousRelayId;
    }

ALLOC_NEW_ID:
    auto IdPart = RelayIdManager.Acquire();
    if (!IdPart) {
        return 0;
    }
    auto TimePart = (uint32_t)ServiceTicker();
    auto NewId    = Make64(TimePart, IdPart);

    auto & Context = RelayIdContext[IdPart];
    assert(!Context.OwnerId);
    assert(!Context.ProtectedRelayId);
    assert(!Context.ProtectionStartTimestampMS);
    assert(!xListNode::IsLinked(Context));
    Context.ProtectedRelayId = NewId;
    Context.OwnerId          = OwnerId;

    DEBUG_LOG("NewRelayId: %" PRIx64 ", Owner=%" PRIx64 "", Context.ProtectedRelayId, Context.OwnerId);
    return NewId;
}

static void ReleaseRelayServerId(uint64_t NowMS) {
    auto Pred = [KillTimepointMs = NowMS - RELAY_ID_PROTECT_TIMEOUT](const xRelayIdContext & N) {
        return N.ProtectionStartTimestampMS <= KillTimepointMs;
    };
    while (auto P = ProtectedRelayIdContextList.PopHead(Pred)) {
        DEBUG_LOG("Id=%" PRIx64 "", P->ProtectedRelayId);
        auto Index = Low32(P->ProtectedRelayId);
        assert(RelayIdManager.IsInUse(Index));
        RelayIdManager.Release(Index);
        Reset(P->ProtectedRelayId);
        Reset(P->OwnerId);
        Reset(P->ProtectionStartTimestampMS);
    }
}

static bool OnClientPacket(const xTcpServiceClientConnectionHandle & H, xPacketCommandId CmdId, xPacketRequestId, ubyte * PL, size_t PS) {
    DEBUG_LOG();
    if (CmdId != Cmd_AcquireRelayServerId) {
        ++audit::InvalidCommandIdCount;
        return false;
    }

    auto & RelayId = H->UserContext.U64;
    if (RelayId) {
        ++audit::MultipleCommandCount;
        return false;
    }

    auto Req = xAcquireRelayServerId();
    if (!Req.Deserialize(PL, PS)) {
        ++audit::InvalidProtocolCount;
        return false;
    }

    auto PreviousRelayId = Req.PreviousRelayServerId;
    RelayId              = GenerateRelayServerId(H.GetConnectionId(), PreviousRelayId);
    return true;
}

static void OnClientClean(const xTcpServiceClientConnectionHandle & H) {
    DEBUG_LOG();
    auto RelayId = Steal(H->UserContext.U64);
    if (!RelayId) {
        return;
    }
    ProtectRelayServerId(RelayId);
}

int main(int argc, char ** argv) {
    X_VAR xServiceEnvironmentGuard(argc, argv);
    auto  LC = ServiceLoadConfig();
    LC.Require(config::BindAddress4, "BindAddress4");

    X_RESOURCE_GUARD(RelayIdManager);
    X_RESOURCE_GUARD(RelayConnectionService, ServiceIoContext, config::BindAddress4, xObjectIdManager::MaxObjectId);
    RelayConnectionService.OnClientPacket = OnClientPacket;
    RelayConnectionService.OnClientClean  = OnClientClean;

    while (ServiceRunState) {
        ServiceUpdateOnce(RelayConnectionService, ReleaseRelayServerId);
    }

    return 0;
}
