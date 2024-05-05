#include "./dns_cache.hpp"

#include "./dns_job.hpp"

#include <array>
#include <unordered_map>

static constexpr const uint64_t    DnsReqTimeoutMS   = 1'000;
static constexpr const xNetAddress InvalidNetAddress = {};

static auto Threads      = std::array<std::thread, 50>();
static auto RequestPool  = xIndexedStorage<xDnsReq>();
static auto DnsReqTicker = xTicker();
static auto DnsReqList   = xList<xDnsReq>();
static auto DnsRunState  = xRunState();
static auto EnableLocal  = false;

// static auto CacheMap = std::unordered_map<std::string, xDnsCacheNode *>();

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

bool IsLocal(const xNetAddress & Addr) {
	if (Addr.IsV4()) {
		ubyte B0 = Addr.SA4[0];
		ubyte B1 = Addr.SA4[1];
		if (B0 == 10) {
			return true;
		}
		if (B0 == 192 && B1 == 168) {
			return true;
		}
		if (B0 == 172 && (B1 >= 16 && B1 < 31)) {
			return true;
		}
	} else if (Addr.IsV6()) {
		ubyte B0 = Addr.SA6[0];
		ubyte B1 = Addr.SA6[1];
		if ((B0 & 0xFE) == 0xFC) {
			return true;
		}
		if (B0 == 0xFE && (B1 & 0xC0 == 0x80)) {
			return true;
		}
	}
	return false;
}

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
	while (auto PJ = FJL.PopHead()) {
		auto & Job      = static_cast<xDnsJob &>(*PJ);
		auto   ReqIndex = Job.JobCtx.U64;
		auto   ReqPtr   = RequestPool.CheckAndGet(ReqIndex);
		X_DEBUG_PRINTF("ReqPtr=%p, Id=%" PRIx64 "", ReqPtr, ReqIndex);
		if (ReqPtr) {
			const xNetAddress * PA4 = &Job.A4;
			const xNetAddress * PA6 = &Job.A6;
			if (!EnableLocal) {
				if (IsLocal(*PA4)) {
					PA4 = &InvalidNetAddress;
				}
				if (IsLocal(*PA6)) {
					PA6 = &InvalidNetAddress;
				}
			}
			Callback(ReqPtr->Context, *PA4, *PA6);
			RequestPool.Release(ReqIndex);
		}
		DeleteDnsJob(&Job);
	}
	// try remove timeout requests:
	DnsReqTicker.Update();
	auto C = [KillTimepoint = DnsReqTicker - DnsReqTimeoutMS](const xDnsReq & N) { return N.Timestamp < KillTimepoint; };
	while (auto NP = DnsReqList.PopHead(C)) {
		assert(NP == RequestPool.CheckAndGet(NP->Index));
		Callback(NP->Context, {}, {});
		RequestPool.Release(NP->Index);
	}
}

void ShrinkDnsCache() {
	// TODO
}

void InitDnsCache(xNotifier Notifier, xVariable Ctx, bool EnableLocal) {
	RuntimeAssert(DnsRunState.Start());
	::EnableLocal = EnableLocal;
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
		while (auto NP = FinishedJobList.PopHead()) {
			DeleteDnsJob(static_cast<xDnsJob *>(NP));
		}
	} while (false);
	::EnableLocal = false;
	DnsRunState.Finish();
}
