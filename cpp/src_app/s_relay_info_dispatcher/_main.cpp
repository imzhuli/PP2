#include <pp_common/service_runtime.hpp>

xNetAddress IncomeBindAddress4;
xNetAddress OutputBindAddress4;

xTcpService IncomeService;
xTcpService OutputService;

int main(int argc, char ** argv) {

    X_VAR xServiceRuntimeEnvGuard(argc, argv);
    auto  LC = RuntimeEnv.LoadConfig();
    LC.Require(IncomeBindAddress4, "IncomeBindAddress4");
    LC.Require(OutputBindAddress4, "OutputBindAddress4");

    X_GUARD(IncomeService, ServiceIoContext, IncomeBindAddress4, 10'0000);
    X_GUARD(OutputService, ServiceIoContext, OutputBindAddress4, 10'0000);

    while (ServiceRunState) {
        ServiceUpdateOnce(IncomeService, OutputService);
    }

    return 0;
}
