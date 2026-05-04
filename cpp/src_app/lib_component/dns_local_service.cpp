#include "./dns_local_service.hpp"

static constexpr const size_t DNS_REQUEST_POOL_SIZE  = 10000;
static constexpr const size_t DNS_QUERY_THREAD_COUNT = 20;

bool xDnsLocalService::Init() {
    if (!DnsRequestPool.Init(DNS_REQUEST_POOL_SIZE)) {
        return false;
    }
    QueryThreadsRunState.Start();

    for (size_t I = 0; I < DNS_QUERY_THREAD_COUNT; ++I) {
        QueryThreads.push_back(std::thread([this] {
            QueryThreadFunc();
        }));
    }
    return true;
}

void xDnsLocalService::Clean() {
    QueryThreadsRunState.Stop();
    for (auto & T : QueryThreads) {
        T.join();
    }
    Reset(QueryThreads);
    QueryThreadsRunState.Finish();

    DnsRequestPool.Clean();
}

void xDnsLocalService::Tick(uint64_t NowMS) {
    LocalTicker.Update(NowMS);
}

void xDnsLocalService::QueryThreadFunc() {
    while (QueryThreadsRunState) {
        std::this_thread::sleep_for(1s);
    }
}

bool xDnsLocalService::ResolveDns(const xDnsRequest & Request, xDnsReultFuture & Future) {
    auto HintIter = CacheMap.find(Request.HostnameView);
    if (HintIter == CacheMap.end()) {

        auto RequestNodeId = DnsRequestPool.Acquire();
        if (!RequestNodeId) {
            return false;
        }
        auto RequestNode   = DnsRequestPool[RequestNodeId];
        RequestNode.NodeId = RequestNodeId;

        auto [Iter, Inserted] = CacheMap.insert(std::make_pair(std::string(Request.HostnameView), nullptr));
        assert(Inserted);
        Pass(Inserted);

        HintIter = Iter;
        return true;
    }
    auto & Node = *HintIter;

    (void)Node;
    return false;
}

void xDnsLocalService::PostDnsRequest(const xDnsRequest & Request) {
}
