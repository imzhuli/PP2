#include "../lib_component/warning_report.hpp"
#include "../lib_util/local_relay_info_manager.hpp"

#include <pp_common/_common.hpp>

int main(int argc, char ** argv) {
    X_VAR xServiceEnvironmentGuard(argc, argv, ServiceConsoleLogger);

    return 0;
}
