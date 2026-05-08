#include "../lib_component/auth_local_service.hpp"
#include "../lib_component/dns_local_service.hpp"
#include "../lib_component/pa_service.hpp"
#include "../lib_component/relay_local_device_service.hpp"

#include <pp_common/service_runtime.hpp>

static auto LocalAuthService   = xAuthLocalService();
static auto LocalRelayService  = xRelayLocalBindingService();
static auto ProxyAccessService = xProxyAccessService();
static auto LocalDnsService    = xDnsLocalService();
static auto LocalAuditTimeout =
#ifndef NDEBUG
    1min;
#else
    10min;
#endif

[[maybe_unused]] static const std::vector<std::pair<xNetAddress, xNetAddress>>
    TestBindingPairList = {
        std::make_pair(xNetAddress::Parse("10.0.0.7"), xNetAddress::Parse("175.178.22.69")),
        std::make_pair(xNetAddress::Parse("2402:4e00:101a:f300:0:9f95:4b15:c0db"), xNetAddress::Parse("2402:4e00:101a:f300:0:9f95:4b15:c0db")),
    };
[[maybe_unused]] static std::string OutputDnsResult(const xDnsReultFuture * F) {
    X_RUNTIME_ASSERT(F);
    if (!F->Result) {
        return "Invalid result";
    }
    auto OS = std::ostringstream();
    OS << "< Address4: " << F->Result->A4.ToString() << ">";
    OS << "< Address6: " << F->Result->A6.ToString() << ">";
    return OS.str();
}

static auto ProxyAccessBindAddress4      = xNetAddress{};
static auto ProxyAccessBindAddress6      = xNetAddress{};
static auto ProxyAccessBindUdpAddress4   = xNetAddress{};
static auto ProxyAccessBindUdpAddress6   = xNetAddress{};
static auto ProxyAccessExportUdpAddress4 = xNetAddress{};
static auto ProxyAccessExportUdpAddress6 = xNetAddress{};
static auto LocalRelayServerId           = (uint64_t)0;
static auto LocalBindingDeviceFile       = std::string{};

static void LoadConfig() {
    auto filename = "./test_assets/local_pa_relay.ini";
    auto CL       = xel::xConfigLoader(filename);
    CL.Optional(ProxyAccessBindAddress4, "PA_BindAddress4");
    CL.Optional(ProxyAccessBindAddress6, "PA_BindAddress6");

    CL.Optional(ProxyAccessBindUdpAddress4, "PA_BindUdpAddress4");
    CL.Optional(ProxyAccessBindUdpAddress6, "PA_BindUdpAddress6");
    CL.Optional(ProxyAccessExportUdpAddress4, "PA_ExportUdpAddress4");
    CL.Optional(ProxyAccessExportUdpAddress6, "PA_ExportUdpAddress6");

    CL.Require(LocalRelayServerId, "LocalRelayServerId");
    CL.Require(LocalBindingDeviceFile, "LocalBindingDeviceFile");

    Logger->I("Begin Config");
    Logger->I("BindAddress4=%s", ProxyAccessBindAddress4.ToString().c_str());
    Logger->I("BindAddress6=%s", ProxyAccessBindAddress6.ToString().c_str());

    Logger->I("ProxyAccessBindUdpAddress4=%s", ProxyAccessBindUdpAddress4.ToString().c_str());
    Logger->I("ProxyAccessBindUdpAddress6=%s", ProxyAccessBindUdpAddress6.ToString().c_str());
    Logger->I("ProxyAccessExportUdpAddress4=%s", ProxyAccessExportUdpAddress4.ToString().c_str());
    Logger->I("ProxyAccessExportUdpAddress6=%s", ProxyAccessExportUdpAddress6.ToString().c_str());

    Logger->I("LocalRelayServerId=%" PRIu64 "", LocalRelayServerId);
    Logger->I("Finish Config");
}

int main(int argc, char ** argv) {
    X_VAR xServiceEnvironmentGuard(argc, argv, ServiceConsoleLogger);
    LoadConfig();

    X_RESOURCE_GUARD_ASSERTED(LocalAuthService, "./test_assets/");
    X_RESOURCE_GUARD_ASSERTED(LocalRelayService, LocalRelayServerId, LocalBindingDeviceFile);
    X_RESOURCE_GUARD_ASSERTED(ProxyAccessService, ProxyAccessBindAddress4, ProxyAccessBindAddress6);
    X_RESOURCE_GUARD_ASSERTED(LocalDnsService);

    if (ProxyAccessBindUdpAddress4) {
        X_RUNTIME_ASSERT(ProxyAccessExportUdpAddress4);
        ProxyAccessService.EnableUdp4(ProxyAccessBindUdpAddress4, ProxyAccessExportUdpAddress4);
    }
    if (ProxyAccessExportUdpAddress6) {
        X_RUNTIME_ASSERT(ProxyAccessExportUdpAddress6);
        ProxyAccessService.EnableUdp6(ProxyAccessBindUdpAddress6, ProxyAccessExportUdpAddress6);
    }
    ProxyAccessService.BindAuthService(&LocalAuthService);
    LocalRelayService.BindDnsService(&LocalDnsService);

    auto AuditTimer = xTimer();
    while (ServiceRunState) {
        ServiceUpdateOnce(LocalRelayService, ProxyAccessService, LocalDnsService);
        if (AuditTimer.TestAndTag(LocalAuditTimeout)) {
            AuditLogger->I("LocalAudit:\n%s%s%s", ProxyAccessService.OutputAudit().c_str(), LocalRelayService.OutputAudit().c_str(), LocalDnsService.OutputAudit().c_str());
        }
    }
}