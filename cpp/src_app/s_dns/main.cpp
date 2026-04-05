#include <ares.h>

#include <pp_common/service_runtime.hpp>
#include <unordered_map>

namespace Audit {
    [[maybe_unused]] static size_t CachedSuccessResultCount = 0;
    [[maybe_unused]] static size_t CachedInvalieResultCount = 0;
};  // namespace Audit

struct xDnsRequest : xListNode {
    xTcpServiceClientConnectionHandle SourceHandle;
    uint64_t                          SourceRequestId;
};

struct xDnsResult : xListNode {
    std::string                   Hostname;
    std::vector<xel::xNetAddress> AddressList;
};

struct xDnsNode4 : xListNode {
    uint64_t                      LastResultTimestampMS;
    std::vector<xel::xNetAddress> AddressList;
};
struct xDnsNode6 : xListNode {
    uint64_t                      LastResultTimestampMS;
    std::vector<xel::xNetAddress> AddressList;
};

struct xDnsNode
    : xDnsNode4
    , xDnsNode6 {
    xList<xDnsRequest> PendingReauestIds;
};

static constexpr const size_t MAX_CONNECTION = 5'000;

//

static auto BindAddress   = xel::xNetAddress::Parse("0.0.0.0:1053");
static auto Service       = xel::xTcpService();
static auto DnsCacheMap   = std::unordered_map<std::string, xDnsNode>();
static auto DnsResultList = xList<xDnsResult>();

static void DnsThread() {
    while (ServiceRunState) {
        printf("DnsThread\n");
        std::this_thread::sleep_for(1s);
    }
}

int main(int argc, char ** argv) {
    X_VAR xServiceEnvironmentGuard(argc, argv);
    auto  LC = ServiceLoadConfig();

    X_RESOURCE_GUARD_ASSERTED(Service, ServiceIoContext, BindAddress, MAX_CONNECTION);
    X_SCOPE_GUARD([] { ares_library_init(ARES_LIB_INIT_ALL); }, [] { ares_library_cleanup(); });
    X_THREAD(DnsThread);

    while (ServiceRunState) {
        ServiceUpdateOnce(Service);
    }

    return 0;
}
