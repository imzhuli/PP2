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

// config
static auto ClientEntryFile        = std::string{};
static auto SmallServerListServer  = xNetAddress{};
static auto LocalRelayServerId     = (uint64_t)0;
static auto LocalAuthFilePath      = std::string();
static auto LocalBindingDeviceFile = std::string{};

static auto ClientEntryBufferSize = (size_t)0;
static auto LocalDeviceBufferSize = (size_t)0;

static void LoadConfig() {
    auto CL = ServiceEnvironment.LoadConfig();
    CL.Require(ClientEntryFile, "ClientEntryFile");

    CL.Require(LocalRelayServerId, "LocalRelayServerId");
    CL.Require(LocalBindingDeviceFile, "LocalBindingDeviceFile");
    CL.Require(LocalAuthFilePath, "LocalAuthFilePath");

    CL.Require(SmallServerListServer, "SmallServerListServer");

    CL.Optional(ClientEntryBufferSize, "ClientEntryBufferSize");
    CL.Optional(LocalDeviceBufferSize, "LocalDeviceBufferSize");

    Logger->I("Begin Config");
    Logger->I("LocalRelayServerId=%" PRIu64 "", LocalRelayServerId);
    Logger->I("Finish Config");
}

int main(int argc, char ** argv) {
    X_VAR xServiceEnvironmentGuard(argc, argv);
    LoadConfig();

    X_RESOURCE_GUARD_ASSERTED(LocalAuthService, LocalAuthFilePath);
    X_RESOURCE_GUARD_ASSERTED(LocalRelayService, LocalRelayServerId, LocalBindingDeviceFile);
    X_RESOURCE_GUARD_ASSERTED(ProxyAccessService, ClientEntryFile);
    X_RESOURCE_GUARD_ASSERTED(LocalDnsService);
    X_RESOURCE_GUARD_ASSERTED(AuditService, SmallServerListServer, SmallServerListServer.Decay());

    LocalAuthService.BindAuditService(&AuditService);
    ProxyAccessService.BindAuthService(&LocalAuthService);
    ProxyAccessService.BindDeviceLocatorService(&LocalRelayService);
    ProxyAccessService.BindRelayService(&LocalRelayService);
    ProxyAccessService.BindTargetReportService(&AuditService);
    LocalRelayService.BindProxyService(&ProxyAccessService);
    LocalRelayService.BindDnsService(&LocalDnsService);

    if (ClientEntryBufferSize) {
        ProxyAccessService.SetClientBufferSize(ClientEntryBufferSize);
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