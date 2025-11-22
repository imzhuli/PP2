#include "../lib_util/service_bootstrap.hpp"

#include <pp_common/service_runtime.hpp>
#include <pp_protocol/command.hpp>
#include <pp_protocol/device/check_address.hpp>

static void OnTcpClientConnected(const xTcpServiceClientConnectionHandle & Handle) {
    auto PeerAddress = Handle.GetRemoteAddress();
    Logger->I("NewConnection: Id=%" PRIx64 ", Address=%s", Handle.GetConnectionId(), PeerAddress.ToString().c_str());
    auto Push              = xPP_AddressAutoPush();
    Push.ConnectionAddress = PeerAddress.Ip();
    Handle.PostMessage(Cmd_DV_RL_AddressPush, 0, Push);
}

static void OnUdpPacket(const xUdpServiceChannelHandle & Handle, xPacketCommandId CommandId, xPacketRequestId, ubyte *, size_t) {
    if (CommandId != Cmd_DV_RL_AddressChallenge) {
        return;
    }
    auto Push              = xPP_AddressAutoPush();
    Push.ConnectionAddress = Handle.GetRemoteAddress().Ip();
    Handle.PostMessage(Cmd_DV_RL_AddressPush, 0, Push);
}

static xTcpService TestAddressServer4;
static xTcpService TestAddressServer6;
static xUdpService TestAddressUdpServer4;
static xUdpService TestAddressUdpServer6;

int main(int argc, char ** argv) {

    auto REG = xServiceRuntimeEnvGuard(argc, argv);
    auto CL  = RuntimeEnv.LoadConfig();

    auto BindAddress4 = xNetAddress();
    auto BindAddress6 = xNetAddress();

    CL.Require(BindAddress4, "BindAddress4");
    CL.Require(BindAddress6, "BindAddress6");

    auto V4Enabled = (bool)BindAddress4;
    auto V6Enabled = (bool)BindAddress6;
    if (!V4Enabled && !V6Enabled) {
        Logger->F("no available bind address");
    }

    TestAddressServer4.OnClientConnected = OnTcpClientConnected;
    TestAddressServer6.OnClientConnected = OnTcpClientConnected;

    TestAddressUdpServer4.OnPacketCallback = OnUdpPacket;
    TestAddressUdpServer6.OnPacketCallback = OnUdpPacket;

    X_COND_GUARD(V4Enabled, TestAddressServer4, ServiceIoContext, BindAddress4, 1024);
    X_COND_GUARD(V4Enabled, TestAddressUdpServer4, ServiceIoContext, BindAddress4);
    X_COND_GUARD(V6Enabled, TestAddressServer6, ServiceIoContext, BindAddress6, 1024);
    X_COND_GUARD(V6Enabled, TestAddressUdpServer6, ServiceIoContext, BindAddress6);

    while (ServiceRunState) {
        ServiceUpdateOnce();
        if (V4Enabled) {
            TestAddressServer4.Tick(ServiceTicker());
        }
        if (V6Enabled) {
            TestAddressServer6.Tick(ServiceTicker());
        }
    }

    return 0;
    //
}
