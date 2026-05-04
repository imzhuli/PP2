#include "./dns_local_service.hpp"

#include <pp_common/service_runtime.hpp>

static constexpr const size_t   DNS_REQUEST_POOL_SIZE        = 3000;
static constexpr const size_t   DNS_SOURCE_REQUEST_POOL_SIZE = 20000;
static constexpr const size_t   DNS_QUERY_THREAD_COUNT       = 20;
static constexpr const uint64_t DNS_QUERY_TIMEOUT_MS         = 5'000;
static constexpr const uint64_t DNS_CACHE_TIMEOUT_MS         = 30 * 60'000;

bool xDnsLocalService::Init() {
    if (!DnsRequestPool.Init(DNS_REQUEST_POOL_SIZE)) {
        return false;
    }
    if (!DnsSourceRequestPool.Init(DNS_SOURCE_REQUEST_POOL_SIZE)) {
        DnsRequestPool.Clean();
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

    DnsSourceRequestPool.Clean();
    DnsRequestPool.Clean();
}

void xDnsLocalService::Tick(uint64_t NowMS) {
    LocalTicker.Update(NowMS);
    ProcessRequestTimeoutCacheNodes();
    ProcessTimeoutCacheNodes();
}

void xDnsLocalService::ProcessRequestTimeoutCacheNodes() {
    auto Cond = [this, KillTimestamp = LocalTicker() - DNS_QUERY_TIMEOUT_MS](const xDnsLocalCacheNode & N) {
        assert(!N.QueryFinished);
        return N.TimestampMS <= KillTimestamp;
    };
    while (auto P = CacheRequestTimeoutList.PopHead(Cond)) {
        assert(!P->Result.has_value());
        while (auto SourceRequest = P->PendingSourceRequestList.PopHead()) {
            if (auto F = SourceRequest->FutureHandle.Get<xDnsReultFuture>()) {
                F->SetReady();
            }
            DnsSourceRequestPool.Release(SourceRequest->SourceRequestNodeId);
        }
        P->QueryFinished = true;
        CacheTimeoutList.AddTail(*P);
    }
}

void xDnsLocalService::ProcessTimeoutCacheNodes() {
    auto Cond = [this, KillTimestamp = LocalTicker() - DNS_CACHE_TIMEOUT_MS_TIMEOUT_MS](const xDnsLocalCacheNode & N) {
        assert(!N.QueryFinished);
        return N.TimestampMS <= KillTimestamp;
    };
    while (auto P = CacheRequestTimeoutList.PopHead(Cond)) {
        assert(P->QueryFinished);
        assert(P->PendingSourceRequestList.IsEmpty());
        CacheTimeoutList.AddTail(*P);
    }
}

void xDnsLocalService::QueryThreadFunc() {
    while (QueryThreadsRunState) {
        std::this_thread::sleep_for(1s);
    }
}

bool xDnsLocalService::ResolveDns(const xDnsRequest & Request, xDnsReultFuture & Future) {
    auto HintIter = CacheMap.find(Request.HostnameView);
    if (HintIter == CacheMap.end()) {
        auto Hostname = std::string(Request.HostnameView);
        DEBUG_LOG("new query, hostname=%s", Hostname.c_str());

        auto SourceRequestNodeId = DnsSourceRequestPool.Acquire();
        if (!SourceRequestNodeId) {
            return false;
        }
        auto & SourceRequestNode              = DnsSourceRequestPool[SourceRequestNodeId];
        SourceRequestNode.SourceRequestNodeId = SourceRequestNodeId;
        SourceRequestNode.FutureHandle        = Future;

        auto RequestNodeId = DnsRequestPool.Acquire();
        if (!RequestNodeId) {
            DnsSourceRequestPool.Release(SourceRequestNodeId);
            return false;
        }
        auto & RequestNode              = DnsRequestPool[RequestNodeId];
        RequestNode.RequestNodeId       = RequestNodeId;
        RequestNode.SourceRequestNodeId = SourceRequestNodeId;
        RequestNode.Hostname            = Hostname;

        auto [Iter, Inserted] = CacheMap.insert(std::make_pair(Hostname, nullptr));
        assert(Inserted);
        Pass(Inserted);
        auto & CacheNode       = Iter->second;
        CacheNode              = new xDnsLocalCacheNode();
        CacheNode->Hostname    = Hostname;
        CacheNode->TimestampMS = LocalTicker();
        CacheNode->PendingSourceRequestList.AddTail(SourceRequestNode);
        CacheRequestTimeoutList.AddTail(*CacheNode);

        RequestEvent.Notify([this, &RequestNode] {
            RequestList.AddTail(RequestNode);
        });

        return true;
    }

    auto & CacheNode = HintIter->second;
    assert(CacheNode->TimestampMS);
    if (!CacheNode->QueryFinished) {  // append request
        DEBUG_LOG("unfinished query, hostname=%s", std::string(Request).c_str());

        auto SourceRequestNodeId = DnsSourceRequestPool.Acquire();
        if (!SourceRequestNodeId) {
            return false;
        }
        auto & SourceRequestNode              = DnsSourceRequestPool[SourceRequestNodeId];
        SourceRequestNode.SourceRequestNodeId = SourceRequestNodeId;
        SourceRequestNode.FutureHandle        = Future;
        CacheNode->PendingSourceRequestList.AddTail(SourceRequestNode);
        return true;
    }

    // has result:
    if (CacheNode->Result.has_value()) {
        auto Result       = xDnsResult();
        auto CachedResult = *CacheNode->Result;
        if (CachedResult.A4List.size()) {
            if (++CachedResult.LastA4ResultIndex >= CachedResult.A4List.size()) {
                CachedResult.LastA4ResultIndex = 0;
            }
            Result.A4 = CachedResult.A4List[CachedResult.LastA4ResultIndex];
        }
        if (CachedResult.A6List.size()) {
            if (++CachedResult.LastA6ResultIndex >= CachedResult.A6List.size()) {
                CachedResult.LastA6ResultIndex = 0;
            }
            Result.A6 = CachedResult.A6List[CachedResult.LastA6ResultIndex];
        }
        Future.Result = Result;
        DEBUG_LOG("Cached result: Hostname=%s, A4=%s, A6=%s", std::string(Request).c_str(), Result.A4.IpToString().c_str(), Result.A6.IpToString().c_str());
    } else {
        DEBUG_LOG("Cached result: Hostname=%s, NotResolved", std::string(Request).c_str());
    }

    Future.SetReady();
    return true;
}