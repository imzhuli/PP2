#include "./_config.hpp"
#include "./ip_location.hpp"

#include <pp_common/service_runtime.hpp>
#include <pp_protocol/command.hpp>
#include <pp_protocol/p_device_address6_challenge.hpp>

static xel::xUdpService      GetRelayService4;
static xel::xUdpService      Address6ChallengeService;
static xCC_IpLocationManager IpLocationManager;

static void OnUdpPacket(const xUdpServiceChannelHandle & Handle, xPacketCommandId CommandId, xPacketRequestId, ubyte *, size_t) {
    if (CommandId != Cmd_DV_CC_GetAddressKey) {
        return;
    }
    auto Push    = xPP_DeviceAddress6ChallengeResp();
    Push.Address = Handle.GetRemoteAddress().Ip();
    Handle.PostMessage(Cmd_DV_CC_GetAddressKeyResp, 0, Push);
}

int main(int argc, char ** argv) {
    X_VAR xServiceEnvironmentGuard(argc, argv, ServiceNoLogger);

    auto LC = ServiceEnvironment.LoadConfig();
    LC.Require(BindAddress4, "BindAddress4");
    LC.Require(BindAddress6, "BindAddress6");

    Daemonize();

    Address6ChallengeService.OnPacketCallback = &OnUdpPacket;

    while (ServiceRunState) {
        ServiceUpdateOnce();
    }
    return 0;
}
