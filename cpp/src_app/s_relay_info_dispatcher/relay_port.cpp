#include "./relay_port.hpp"

#include "./config.hpp"

#include <pp_protocol/command.hpp>
#include <pp_protocol/internal/relay_info.hpp>

static constexpr const uint64_t RELAY_HANDSHAKE_TIMEOUT_MS = 2'000;
static constexpr const uint64_t RELAY_LINGERING_TIMEOUT_MS = 180'000;

enum struct eRDI_RelayState : uint16_t {
    Unspecified = 0,
    WaitForInfo = 1,
    Ready       = 2,
    Lingering   = 3,
};

struct xRID_RelayPortTimeoutNode : xListNode {
    uint64_t InitTimestampMS = 0;
};

struct xRID_RelayPort : xRID_RelayPortTimeoutNode {
    eRDI_RelayState                   State                = eRDI_RelayState::Unspecified;
    xTcpServiceClientConnectionHandle ConnectionHandle     = {};
    uint64_t                          LingerTimestampMS    = 0;
    uint64_t                          RelayServerRuntimeId = 0;
    xNetAddress                       RelayDevicePortAddress;
    xNetAddress                       RelayClientPortAddress;
};
static xRID_RelayPort        RelayPortMap[MAX_RELAY_CONTEXT_COUNT];
static xList<xRID_RelayPort> HandshakeRelayList;
static xList<xRID_RelayPort> CleanRelayList;

static xTcpService RelayService;

static void OnRelayConnected(const xTcpServiceClientConnectionHandle & H) {
    DEBUG_LOG();
}

static bool OnRegisterRelayServer(const xTcpServiceClientConnectionHandle & H, ubyte * PL, size_t PS) {
    auto ServerId = H->UserContext.IX;
    if (ServerId) {  // invalid connection
        Logger->E("OnRegisterRelayServer: multiple ServerInfo");
        return false;
    }

    auto PP = xPP_RelayInfoRegister();
    if (!PP.Deserialize(PL, PS)) {
        Logger->E("OnRegisterRelayServer: invalid protocol");
        return false;
    }

    auto ServerIndex = ExtractIndexFromServerId(PP.ExportRelayServerInfo.ServerId);
    if (ServerIndex >= MAX_RELAY_CONTEXT_COUNT) {
        Logger->E("OnRegisterRelayServer: server index overflow: %" PRIu32 "", ServerIndex);
        return false;
    }
    auto PInfo = &RelayPortMap[ServerIndex];
    if (PInfo->State != eRDI_RelayState::Unspecified) {
        Logger->E("OnRegisterRelayServer: server info conflict");
        return false;
    }

    auto & SourceInfo             = PP.ExportRelayServerInfo;
    PInfo->ConnectionHandle       = H;
    PInfo->RelayServerRuntimeId   = SourceInfo.ServerId;
    PInfo->RelayDevicePortAddress = SourceInfo.DevicePortAddress;
    PInfo->RelayClientPortAddress = SourceInfo.ClientPortAddress;

    AuditLogger->I("New RelayServer added: %" PRIu64 ", DP=%s, DC=%s",  //
                   PInfo->RelayServerRuntimeId,                         //
                   PInfo->RelayDevicePortAddress.ToString().c_str(), PInfo->RelayClientPortAddress.ToString().c_str());

    return true;
}

static bool OnRelayPacket(const xTcpServiceClientConnectionHandle & H, xPacketCommandId CId, xPacketRequestId RId, ubyte * PL, size_t PS) {
    switch (CId) {
        case Cmd_RelayInfoRegister:
            return OnRegisterRelayServer(H, PL, PS);
        default:
            break;
    }
    return true;
}

static void OnRelayKeepAlive(const xTcpServiceClientConnectionHandle & H) {
    DEBUG_LOG();
    auto ServerId = H->UserContext.IX;
    if (!ServerId) {  // invalid connection
        Logger->E("connection w/o relay info, closing");
        H.Kill();
        return;
    }
    // Todo: broad cast relay info to "new observer"
}

static void OnRelayClean(const xTcpServiceClientConnectionHandle & H) {
    DEBUG_LOG();
}

void InitRelayPort() {
    RuntimeAssert(RelayService.Init(ServiceIoContext, RelayPortBindAddress4, MAX_RELAY_CONTEXT_COUNT));
    RelayService.OnClientConnected = OnRelayConnected;
    RelayService.OnClientPacket    = OnRelayPacket;
    RelayService.OnClientKeepAlive = OnRelayKeepAlive;
    RelayService.OnClientClean     = OnRelayClean;
}

void CleanRelayPort() {
    RelayService.Clean();
    for (auto & P : RelayPortMap) {
        if (Steal(P.RelayServerRuntimeId)) {
            Pass();
        }
    }
}

void RelayPortTick(uint64_t NowMS) {
    RelayService.Tick(NowMS);
}
