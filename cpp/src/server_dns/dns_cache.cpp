#include "./dns_cache.hpp"

#include "./dns_job.hpp"

#include <array>
#include <unordered_map>

static constexpr const uint64_t DnsReqTimeoutMS = 1'000;

static auto Threads      = std::array<std::thread, 50>();
static auto RequestPool  = xIndexedStorage<xDnsReq>();
static auto DnsReqTicker = xTicker();
static auto DnsReqList   = xList<xDnsReq>();
static auto DnsRunState  = xRunState();

static auto CacheMap = std::unordered_map<std::string, xDnsCacheNode *>();

static auto RPG = xResourceGuard(RequestPool, 10'000);

// [[maybe_unused]] bool GetCachedDnsInfo(const std::string & Hostname, xNetAddress & Output) {
// 	Output    = {};
// 	auto Iter = CacheMap.find(Hostname);
// 	if (Iter == CacheMap.end()) {
// 		return false;
// 	}
// 	auto PN = Iter->second;
// 	if (PN->State <= xDnsCacheNode::FIRST_QUERY) {
// 		return false;
// 	}
// 	Output = PN->A4;
// 	return true;
// }

bool PostDnsRequest(const std::string & Hostname, xVariable Ctx) {
	auto ReqIndex = RequestPool.Acquire();
	if (!ReqIndex) {
		return false;
	}

	auto & Req    = RequestPool[ReqIndex];
	Req.Index     = ReqIndex;
	Req.Timestamp = DnsReqTicker;
	Req.Context   = Ctx;
	DnsReqList.AddTail(Req);

	auto JobPtr = NewDnsJob(Hostname, { .U64 = Req.Index });
	if (!JobPtr) {
		RequestPool.Release(Req.Index);
		return false;
	}
	NewDnsJobQueue.PostJob(*JobPtr);
	return true;
}

void DispatchResults(xDnsResultCallback Callback) {
	xJobList FJL = {};
	do {
		auto G = xSpinlockGuard(FinishedJobListLock);
		FJL.GrabListTail(FinishedJobList);
	} while (false);
	for (auto & Node : FJL) {
		auto & Job      = static_cast<xDnsJob &>(Node);
		auto   ReqIndex = Job.JobCtx.U64;
		auto   ReqPtr   = RequestPool.CheckAndGet(ReqIndex);
		if (ReqPtr) {
			Callback(ReqPtr->Context, Job.A4);
			RequestPool.Release(ReqIndex);
		}
		DeleteDnsJob(&Job);
	}
	// try remove timeout requests:
	DnsReqTicker.Update();
	auto KillTimepoint = DnsReqTicker - DnsReqTimeoutMS;
	for (auto & N : DnsReqList) {
		auto ReqPtr = &N;
		if (ReqPtr->Timestamp > KillTimepoint) {
			break;
		}
		assert(ReqPtr == RequestPool.CheckAndGet(N.Index));
		Callback(ReqPtr->Context, {});
		RequestPool.Release(N.Index);
	}
}

void ShrinkDnsCache() {
	// TODO
}

void InitDnsCache(xNotifier Notifier, xVariable Ctx) {
	RuntimeAssert(DnsRunState.Start());
	for (auto & T : Threads) {
		T = std::thread(DnsWorkerThread, Notifier, Ctx);
	}
}

void CleanDnsCache() {
	DnsRunState.Stop();
	NewDnsJobQueue.PostWakeupN(Threads.size());
	for (auto & T : Threads) {
		T.join();
	}
	do {
		auto G = xSpinlockGuard(FinishedJobListLock);
		for (auto & Node : FinishedJobList) {
			auto & J = static_cast<xDnsJob &>(Node);
			DeleteDnsJob(&J);
		}
	} while (false);
	DnsRunState.Finish();
}
