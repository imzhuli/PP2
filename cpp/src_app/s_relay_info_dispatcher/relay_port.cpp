#include "./relay_port.hpp"

#include "./config.hpp"

#include <pp_protocol/command.hpp>

static constexpr const uint64_t RELAY_LINGERING_TIMEOUT_MS = 180'000;

enum struct eRDI_RelayState : uint16_t {
    Unspecified = 0,
    WaitForInfo = 1,
    Ready       = 2,
    Lingering   = 3,
};

struct xRID_RelayPort : xListNode {
    eRDI_RelayState State                = eRDI_RelayState::Unspecified;
    uint64_t        LastConnectionId     = 0;
    uint64_t        LingerTimestampMS    = 0;
    uint64_t        RelayServerRuntimeId = 0;
    xNetAddress     RelayDevicePortAddress;
    xNetAddress     RelayClientPortAddress;
};
static xRID_RelayPort        RelayPortMap[MAX_RELAY_CONTEXT_COUNT];
static xList<xRID_RelayPort> HandshakeRelayList;
static xList<xRID_RelayPort> CleanRelayList;

static xTcpService RelayService;

static void OnRelayConnected(const xTcpServiceClientConnectionHandle & H) {
    DEBUG_LOG();
}

static bool OnRelayPacket(const xTcpServiceClientConnectionHandle & H, xPacketCommandId CId, xPacketRequestId RId, ubyte * PL, size_t PS) {
    return true;
}

static void OnRelayKeepAlive(const xTcpServiceClientConnectionHandle & H) {
    DEBUG_LOG();
    auto Index = H->UserContext.IX;
    if (!Index) {
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
