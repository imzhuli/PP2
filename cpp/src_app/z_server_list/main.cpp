#include <pp_common/service_runtime.hpp>

int main(int argc, char ** argv) {

    X_VAR xServiceRuntimeEnvGuard(argc, argv, false);

    ConsoleLogger->D("Hello world! %s", "xellee");

    return 0;
}
