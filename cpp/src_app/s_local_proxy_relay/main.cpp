#include "../lib_component/auth_local.hpp"
#include "../lib_component/pa_service.hpp"
#include "../lib_component/relay_local_device_service.hpp"

#include <pp_common/service_runtime.hpp>

static auto LocalAuthService   = xAuthLocalService();
static auto RelayLocalService  = xRelayLocalBindingService();
static auto ProxyAccessService = xProxyAccessService();

static const std::vector<std::pair<xNetAddress, xNetAddress>> TestBindingPairList = {
    std::make_pair(xNetAddress::Parse("10.0.0.7"), xNetAddress::Parse("175.178.22.69")),
    std::make_pair(xNetAddress::Parse("2402:4e00:101a:f300:0:9f95:4b15:c0db"), xNetAddress::Parse("2402:4e00:101a:f300:0:9f95:4b15:c0db")),
};
static auto ProxyAccessBindAddress4 = xNetAddress{};
static auto ProxyAccessBindAddress6 = xNetAddress{};

static void LoadConfig() {
    auto filename = "./test_assets/local_pa_relay.ini";
    auto CL       = xel::xConfigLoader(filename);
    CL.Optional(ProxyAccessBindAddress4, "PA_BindAddress4");
    CL.Optional(ProxyAccessBindAddress6, "PA_BindAddress6");

    DEBUG_LOG("BindAddress4=%s", ProxyAccessBindAddress4.ToString().c_str());
    DEBUG_LOG("BindAddress6=%s", ProxyAccessBindAddress6.ToString().c_str());
}

int main(int argc, char ** argv) {
    X_VAR xServiceEnvironmentGuard(argc, argv, ServiceConsoleLogger);
    LoadConfig();

    X_RESOURCE_GUARD_ASSERTED(LocalAuthService, "./test_assets/");
    X_RESOURCE_GUARD_ASSERTED(RelayLocalService, TestBindingPairList);
    X_RESOURCE_GUARD_ASSERTED(ProxyAccessService, ProxyAccessBindAddress4, ProxyAccessBindAddress6);

    while (ServiceRunState) {
        ServiceUpdateOnce(LocalAuthService, RelayLocalService, ProxyAccessService);
    }
}