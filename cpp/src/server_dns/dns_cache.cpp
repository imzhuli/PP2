#include "./dns_cache.hpp"

#include <unordered_map>

static auto CacheMap    = std::unordered_map<std::string, xDnsCacheNode *>();
static auto RequestPool = xIndexedStorage<xDnsReq>();
static auto DnsReqList  = xList<xDnsReq>();

static auto RPG = xResourceGuard(RequestPool, 10'000);

bool GetCachedDnsInfo(const std::string & Hostname, xNetAddress & Output) {
	Output    = {};
	auto Iter = CacheMap.find(Hostname);
	if (Iter == CacheMap.end()) {
		return false;
	}
	auto PN = Iter->second;
	if (PN->State <= xDnsCacheNode::FIRST_QUERY) {
		return false;
	}
	Output = PN->A4;
	return true;
}

bool PostDnsRequest(const std::string & Hostname, xVariable Ctx) {
	auto ReqIndex = RequestPool.Acquire();
	if (!ReqIndex) {
		return false;
	}

	auto & Req    = RequestPool[ReqIndex];
	Req.Index     = ReqIndex;
	Req.Timestamp = GetTimestampMS();
	Req.Context   = Ctx;
	DnsReqList.AddTail(Req);

	// TODO: do real request

	return false;
}

void ShrinkDnsCache() {
}
