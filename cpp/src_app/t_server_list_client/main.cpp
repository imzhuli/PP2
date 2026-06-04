#include "../lib_component/audit_service.hpp"

#include <pp_common/service_runtime.hpp>
#include <pp_protocol/command.hpp>
#include <pp_protocol/p_small_server_list.hpp>

static auto TargetAddress       = xNetAddress::Parse("127.0.0.1:10000");
static auto ReporterBindAddress = xNetAddress::Make4();
static auto AuditService        = xAuditService();

int main(int argc, char ** argv) {

    X_VAR xServiceEnvironmentGuard(argc, argv, ServiceConsoleLogger);

    X_RESOURCE_GUARD_ASSERTED(AuditService, TargetAddress, ReporterBindAddress);

    while (ServiceRunState) {
        ServiceUpdateOnce(AuditService);
    }

    return 0;
}
