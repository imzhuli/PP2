#include "../lib_util/service_bootstrap.hpp"

#include <pp_common/service_runtime.hpp>

xServiceBootstrap SB;
eServiceType      MyServiceType    = eServiceType::ServerTest;
xNetAddress       MyServiceAddress = xNetAddress::Parse("127.0.0.1:10300");

int main(int argc, char ** argv) {
    X_VAR xServiceRuntimeEnvGuard(argc, argv, false);

    X_GUARD(SB, ServiceIoContext, xNetAddress::Parse("127.0.0.1:10200"));
    SB.OnServerIdUpdated = [](uint64_t ServerId) { cout << "ServerId updated : " << ServerId << endl; };
    SB.AddServiceRegistration(MyServiceType, MyServiceAddress);

    while (ServiceRunState) {
        ServiceUpdateOnce(SB);
    }

    return 0;
}
