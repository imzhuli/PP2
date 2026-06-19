#include <pp_common/_common.hpp>
#include <pp_common/service_runtime.hpp>
//
#include <absl/container/flat_hash_map.h>

struct xDeviceLocator {
    xAbstractRelayServerInfo RelayServerInfo;
    xAbstractDeviceInfo      DeviceInfo;
};

using xAddress4Key = std::array<uint8_t, 4>;
using xAddress6Key = std::array<uint8_t, 16>;

static auto Address4Map = absl::flat_hash_map<xAddress4Key, xDeviceLocator>();
static auto Address6Map = absl::flat_hash_map<xAddress6Key, xDeviceLocator>();

int main(int argc, char ** argv) {

    Address4Map.reserve(1'000'000);
    Address6Map.reserve(1'000'000);

    return 0;
}