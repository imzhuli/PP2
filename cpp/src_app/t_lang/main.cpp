#include "../lib_util/local_relay_info_manager.hpp"

#include <pp_common/_common.hpp>

static auto M = xLocalRelayInfoManager();

int main(int argc, char ** argv) {
    X_VAR xServiceEnvironmentGuard(argc, argv, ServiceConsoleLogger);

    X_RESOURCE_GUARD(M);

    M.OnAddRelayInfo = [](const xAbstractRelayServerInfo & RelayServerInfo) {
        M.RemoveRelayInfo(RelayServerInfo.Id);
    };

    M.AddRelayInfo({ .Id = 1 }, 1);

    return 0;
}
