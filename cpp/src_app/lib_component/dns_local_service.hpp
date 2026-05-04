#pragma once
#include "./abstract/dns_abstract.hpp"

#include <map>

struct xDnsLocalRequestNode : xListNode {
    uint64_t      NodeId;
    xFutureHandle FutureHandle;
};
using xDnsLocalRequestList = xList<xDnsLocalRequestNode>;

struct xDnsLocalCacheResult {
    std::vector<xNetAddress> A4List;
    std::vector<xNetAddress> A6List;
};

struct xDnsLocalCacheNode : xListNode {
    uint64_t                                   TimestampMS   = 0;
    bool                                       QueryFinished = false;
    std::expected<xDnsLocalCacheResult, xNone> Result        = std::unexpected(None);
    xDnsLocalRequestList                       PendingRequestList;
};
using xDnsLocalCacheTimeoutList = xList<xDnsLocalCacheNode>;

class xDnsLocalService final : public xDnsAbstractService {
public:
    bool Init();
    void Clean();
    void Tick(uint64_t NowMS);

private:
    bool ResolveDns(const xDnsRequest & Request, xDnsReultFuture & Future) override;

private:
    void QueryThreadFunc();
    void PostDnsRequest(const xDnsRequest & Request);

private:
    xTicker                  LocalTicker;
    xel::xRunState           QueryThreadsRunState;
    std::vector<std::thread> QueryThreads;

    xIndexedStorage<xDnsLocalRequestNode>                        DnsRequestPool;
    xIndexedStorage<xDnsLocalCacheNode>                          DnsLocalCachePool;
    std::map<std::string, xDnsLocalCacheNode *, std::less<void>> CacheMap;
    xDnsLocalCacheTimeoutList                                    CacheTimeoutList;
};
