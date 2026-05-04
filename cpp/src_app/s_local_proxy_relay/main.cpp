#include "../lib_component/auth_local_service.hpp"
#include "../lib_component/dns_local_service.hpp"
#include "../lib_component/pa_service.hpp"
#include "../lib_component/relay_local_device_service.hpp"

#include <pp_common/service_runtime.hpp>

static auto LocalAuthService   = xAuthLocalService();
static auto RelayLocalService  = xRelayLocalBindingService();
static auto ProxyAccessService = xProxyAccessService();
static auto DnsService         = xDnsLocalService();

[[maybe_unused]] static const std::vector<std::pair<xNetAddress, xNetAddress>> TestBindingPairList = {
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
    X_RESOURCE_GUARD_ASSERTED(RelayLocalService, LocalRelayServerId, LocalBindingDeviceFile);
    X_RESOURCE_GUARD_ASSERTED(ProxyAccessService, ProxyAccessBindAddress4, ProxyAccessBindAddress6);
    X_RESOURCE_GUARD_ASSERTED(DnsService);

    if (ProxyAccessBindUdpAddress4) {
        X_RUNTIME_ASSERT(ProxyAccessExportUdpAddress4);
        ProxyAccessService.EnableUdp4(ProxyAccessBindUdpAddress4, ProxyAccessExportUdpAddress4);
    }
    if (ProxyAccessExportUdpAddress6) {
        X_RUNTIME_ASSERT(ProxyAccessExportUdpAddress6);
        ProxyAccessService.EnableUdp6(ProxyAccessBindUdpAddress6, ProxyAccessExportUdpAddress6);
    }
    ProxyAccessService.BindAuthService(&LocalAuthService);

    auto TestDnsFuturePool = xFuturePoolManager<xDnsReultFuture>();
    TestDnsFuturePool.Init(10);

    const char * N1 = "www.baidu.com";
    const char * N2 = "www.qq.com";
    const char * N3 = "www.qq.com";

    auto F1 = TestDnsFuturePool.AcquireFuture();
    auto F2 = TestDnsFuturePool.AcquireFuture();
    auto F3 = TestDnsFuturePool.AcquireFuture();

    DnsService.ResolveDns(N1, *F1);
    DnsService.ResolveDns(N2, *F2);
    DnsService.ResolveDns(N3, *F3);

    while (ServiceRunState) {
        ServiceUpdateOnce(RelayLocalService, ProxyAccessService, DnsService);
        if (F1 && F1->IsReady) {
            DEBUG_LOG("Result: %s --> %s", N1, OutputDnsResult(F1).c_str());
            TestDnsFuturePool.ReleaseFuture(Steal(F1));
        }
        if (F2 && F2->IsReady) {
            DEBUG_LOG("Result: %s --> %s", N2, OutputDnsResult(F2).c_str());
            TestDnsFuturePool.ReleaseFuture(Steal(F2));
        }
        if (F3 && F3->IsReady) {
            DEBUG_LOG("Result: %s --> %s", N3, OutputDnsResult(F3).c_str());
            TestDnsFuturePool.ReleaseFuture(Steal(F3));
        }
    }

    F1 ? TestDnsFuturePool.ReleaseFuture(F1) : Pass();
    F2 ? TestDnsFuturePool.ReleaseFuture(F2) : Pass();
    F3 ? TestDnsFuturePool.ReleaseFuture(F3) : Pass();

    TestDnsFuturePool.Clean();
}