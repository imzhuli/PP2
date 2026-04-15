#include "../lib_component/auth_local.hpp"
#include "../lib_component/pa_local.hpp"
#include "../lib_component/relay_local_device_service.hpp"

#include <pp_common/service_runtime.hpp>

static auto LocalAuthService  = xAuthLocalService();
static auto RelayLocalService = xRelayLocalBindingService();

static const std::vector<std::pair<xNetAddress, xNetAddress>> TestBindingPairList = {
    std::make_pair(xNetAddress::Parse("10.0.0.7"), xNetAddress::Parse("175.178.22.69")),
    std::make_pair(xNetAddress::Parse("2402:4e00:101a:f300:0:9f95:4b15:c0db"), xNetAddress::Parse("2402:4e00:101a:f300:0:9f95:4b15:c0db")),
};

int main(int argc, char ** argv) {
    X_VAR xServiceEnvironmentGuard(argc, argv, ServiceConsoleLogger);
    X_RESOURCE_GUARD_ASSERTED(LocalAuthService, "./test_assets/");
    X_RESOURCE_GUARD_ASSERTED(RelayLocalService, TestBindingPairList);

    while (ServiceRunState) {
        ServiceUpdateOnce();
    }
}