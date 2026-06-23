#include "../lib_component/audit_service.hpp"
#include "../lib_component/auth_local_service.hpp"
#include "../lib_component/dns_local_service.hpp"
#include "../lib_component/pa_service.hpp"
#include "../lib_component/relay_local_device_service.hpp"

#include <pp_common/service_runtime.hpp>

static auto LocalAuthService   = xAuthLocalService();
static auto LocalRelayService  = xRelayLocalBindingService();
static auto ProxyAccessService = xProxyAccessService();
static auto LocalDnsService    = xDnsLocalService();
static auto AuditService       = xAuditService();
static auto LocalAuditTimeout =
#ifndef NDEBUG
    1min;
#else
    10min;
#endif

static auto ProxyAccessBindAddress4      = xNetAddress{};
static auto ProxyAccessBindAddress6      = xNetAddress{};
static auto ProxyAccessBindUdpAddress4   = xNetAddress{};
static auto ProxyAccessBindUdpAddress6   = xNetAddress{};
static auto ProxyAccessExportUdpAddress4 = xNetAddress{};
static auto ProxyAccessExportUdpAddress6 = xNetAddress{};
static auto SmallServerListServer        = xNetAddress{};
static auto LocalRelayServerId           = (uint64_t)0;
static auto LocalAuthFilePath            = std::string();
static auto LocalBindingDeviceFile       = std::string{};

static auto ProxyAccessBufferSize = (size_t)0;
static auto LocalDeviceBufferSize = (size_t)0;

static void LoadConfig() {
    auto CL = ServiceEnvironment.LoadConfig();
    CL.Optional(ProxyAccessBindAddress4, "PA_BindAddress4");
    CL.Optional(ProxyAccessBindAddress6, "PA_BindAddress6");

    CL.Optional(ProxyAccessBindUdpAddress4, "PA_BindUdpAddress4");
    CL.Optional(ProxyAccessBindUdpAddress6, "PA_BindUdpAddress6");
    CL.Optional(ProxyAccessExportUdpAddress4, "PA_ExportUdpAddress4");
    CL.Optional(ProxyAccessExportUdpAddress6, "PA_ExportUdpAddress6");

    CL.Require(LocalRelayServerId, "LocalRelayServerId");
    CL.Require(LocalBindingDeviceFile, "LocalBindingDeviceFile");
    CL.Require(LocalAuthFilePath, "LocalAuthFilePath");

    CL.Require(SmallServerListServer, "SmallServerListServer");

    CL.Optional(ProxyAccessBufferSize, "ProxyAccessBufferSize");
    CL.Optional(LocalDeviceBufferSize, "LocalDeviceBufferSize");

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
    X_VAR xServiceEnvironmentGuard(argc, argv);
    LoadConfig();

    X_RESOURCE_GUARD_ASSERTED(LocalAuthService, LocalAuthFilePath);
    X_RESOURCE_GUARD_ASSERTED(LocalRelayService, LocalRelayServerId, LocalBindingDeviceFile);
    X_RESOURCE_GUARD_ASSERTED(ProxyAccessService, ProxyAccessBindAddress4, ProxyAccessBindAddress6);
    X_RESOURCE_GUARD_ASSERTED(LocalDnsService);
    X_RESOURCE_GUARD_ASSERTED(AuditService, SmallServerListServer, SmallServerListServer.Decay());

    if (ProxyAccessBindUdpAddress4) {
        X_RUNTIME_ASSERT(ProxyAccessExportUdpAddress4);
        ProxyAccessService.EnableUdp4(ProxyAccessBindUdpAddress4, ProxyAccessExportUdpAddress4);
    }
    if (ProxyAccessExportUdpAddress6) {
        X_RUNTIME_ASSERT(ProxyAccessExportUdpAddress6);
        ProxyAccessService.EnableUdp6(ProxyAccessBindUdpAddress6, ProxyAccessExportUdpAddress6);
    }
    LocalAuthService.BindAuditService(&AuditService);
    ProxyAccessService.BindAuthService(&LocalAuthService);
    ProxyAccessService.BindDeviceLocatorService(&LocalRelayService);
    ProxyAccessService.BindRelayService(&LocalRelayService);
    ProxyAccessService.BindTargetReportService(&AuditService);
    LocalRelayService.BindProxyService(&ProxyAccessService);
    LocalRelayService.BindDnsService(&LocalDnsService);

    if (ProxyAccessBufferSize) {
        ProxyAccessService.SetClientBufferSize(ProxyAccessBufferSize);
    }
    if (LocalDeviceBufferSize) {
        LocalRelayService.SetDeviceBufferSize(LocalDeviceBufferSize);
    }

    auto AuditTimer = xTimer();
    while (ServiceRunState) {
        ServiceUpdateOnce(LocalRelayService, ProxyAccessService, LocalDnsService, LocalAuthService, AuditService);
        if (AuditTimer.TestAndTag(LocalAuditTimeout)) {
            AuditLogger->I("LocalAudit:\n%s%s%s%s", ProxyAccessService.OutputAudit().c_str(), LocalAuthService.OutputAudit().c_str(), LocalRelayService.OutputAudit().c_str(), LocalDnsService.OutputAudit().c_str());
        }
    }
}