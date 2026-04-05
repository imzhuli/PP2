#include <pp_common/service_runtime.hpp>

int main(int argc, char ** argv) {
    X_VAR xServiceEnvironmentGuard(argc, argv);

    while (ServiceRunState) {
        ServiceUpdateOnce();
    }

    return 0;
}
