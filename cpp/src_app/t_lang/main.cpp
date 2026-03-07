#include "../lib_util/local_relay_info_manager.hpp"

#include <pp_common/_common.hpp>

static auto   M         = xLocalRelayInfoManager();
static auto   NoReentry = xNoReentry();
static size_t Counter   = 10;

void N() {
    X_VAR NoReentry.Scope();
}

void R() {
    X_VAR NoReentry.Scope();

    if (!--Counter) {
        return;
    }
    R();
}

int main(int argc, char ** argv) {
    X_VAR xServiceEnvironmentGuard(argc, argv, ServiceConsoleLogger);

    X_RESOURCE_GUARD(M);

    N();
    R();

    M.OnAddRelayInfo = [](const xAbstractRelayServerInfo & RelayServerInfo) {
        M.RemoveRelayInfo(RelayServerInfo.Id);
    };

    M.AddRelayInfo({ .Id = 1 });

    return 0;
}
