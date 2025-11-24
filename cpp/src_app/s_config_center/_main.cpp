#include "./_config.hpp"
#include "./ip_location.hpp"
#include "./relay_node.hpp"

#include <pp_common/service_runtime.hpp>
#include <pp_protocol/command.hpp>
#include <pp_protocol/device/challenge.hpp>
#include <pp_protocol/internal/challenge_key.hpp>

static xel::xUdpService      Service4;
static xel::xUdpService      Service6;
static xCC_IpLocationManager IpLocationManager;

static const xRelayServerInfoBase * GetRandomDeviceRelayServer4() {
    return nullptr;
}

static const xRelayServerInfoBase * GetRandomDeviceRelayServer6() {
    return nullptr;
}

void OnTerminalChallenge(const xUdpServiceChannelHandle & Handle, xPacketCommandId CmdId, xPacketRequestId RequestId, ubyte * PayloadPtr, size_t PayloadSize) {
    if (CmdId != Cmd_DV_CC_Challenge) {
        Logger->E("invalid command id");
        return;
    }

    // todo: read input
    auto DC = xPP_DeviceChallenge();
    if (!DC.Deserialize(PayloadPtr, PayloadSize)) {
        Logger->E("invalid protocol");
        return;
    }

    auto Under = DC.Extract();

    auto RelayAddress = xNetAddress();
    auto GeoInfo      = xGeoInfo{};
    if (Under.Tcp4Address.Is4()) {
        GeoInfo = IpLocationManager.GetRegionByIp(Under.Tcp4Address.IpToString().c_str());
        auto SI = GetRandomDeviceRelayServer4();
        if (SI) {
            RelayAddress = SI->ExportDeviceAddress4;
        }
    }

    if (!RelayAddress && Under.Tcp6Address.Is6()) {
        auto SI = GetRandomDeviceRelayServer6();
        if (!GeoInfo.CountryId) {
            GeoInfo = IpLocationManager.GetRegionByIp(Under.Tcp4Address.IpToString().c_str());
        }
        if (SI) {
            RelayAddress = SI->ExportDeviceAddress6;
        }
    }

    auto Resp = xPP_DeviceChallengeResp();
    if (RelayAddress) {
        DEBUG_LOG(
            "relay server found: %s, DeviceGeoInfo:%u/%u/%u", RelayAddress.ToString().c_str(), (unsigned)GeoInfo.CountryId, (unsigned)GeoInfo.StateId, (unsigned)GeoInfo.CityId
        );
        auto Key      = RelayAddress.ToString();
        Under.GeoInfo = GeoInfo;

        Resp.Accepted      = true;
        Resp.RelayAddress  = RelayAddress;
        Resp.RelayCheckKey = MakeChallengeKey(Under);
    } else {
        DEBUG_LOG("no relay server found");
    }

    Handle.PostMessage(Cmd_DV_CC_ChallengeResp, RequestId, Resp);
}

int main(int argc, char ** argv) {
    X_VAR xServiceRuntimeEnvGuard(argc, argv);
    X_COND_GUARD(BindAddress4, Service4, ServiceIoContext, BindAddress4);
    X_COND_GUARD(BindAddress6, Service6, ServiceIoContext, BindAddress6);

    while (ServiceRunState) {
        ServiceUpdateOnce();
    }
    return 0;
}
