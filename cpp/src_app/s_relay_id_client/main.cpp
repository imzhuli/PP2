#include "./relay_id_client.hpp"

#include <pp_common/service_runtime.hpp>

namespace config {

    xNetAddress ServerAddress;
    int64_t     PreviousRelayId;

}  // namespace config

static xRelayIdClient RelayIdClient;

int main(int argc, char ** argv) {
    X_VAR xServiceEnvironmentGuard(argc, argv);
    auto  LC = ServiceLoadConfig();
    LC.Require(config::ServerAddress, "ServerAddress");
    LC.Optional(config::PreviousRelayId, "PreviousRelayId");

    X_RESOURCE_GUARD(RelayIdClient, config::ServerAddress, config::PreviousRelayId);
    RelayIdClient.OnRelayIdUpdated = [](uint64_t Id) {
        DEBUG_LOG("New Id=%" PRIu64 "", Id);
    };

    auto T = xel::xTimer();
    while (ServiceRunState && T.Elapsed() < 1s) {
        ServiceUpdateOnce(RelayIdClient);
    }

    return 0;
}
