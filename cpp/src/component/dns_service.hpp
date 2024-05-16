#pragma once
#include "../common/base.hpp"
#include "../common_job/job.hpp"

#include <thread>
#include <vector>

struct xDnsJob : xJobNode {
	xVariable   JobCtx = {};
	std::string Hostname;
	xNetAddress A4;
	xNetAddress A6;
};

using xDnsResultCallback = void(xVariable CallbackContext, xVariable RequestContext, const xNetAddress & A4, const xNetAddress & A6);

// {
// 	X_DEBUG_PRINTF("%" PRIx64 ": ipv4=%s, ipv6=%s", Ctx.U64, A4.IpToString().c_str(), A6.IpToString().c_str());
// 	DnsService.OnHostQueryResult(Ctx, A4, A6);
// }

class xDnsService : xNonCopyable {
public:
	bool Init(xNotifierFunc & WakeupFunc, xVariable WakeupCtx = {});
	void Clean();

	bool Query(const std::string & Hostname, xVariable JobCtx);
	void GetAndDispatchResult(xDnsResultCallback Callback, xVariable CallbackCtx);

protected:
	xDnsJob * NewDnsJob(const std::string & Hostname, xVariable JobCtx);
	void      DeleteDnsJob(const xDnsJob * PJ);
	void      WorkerThreadFunc();

protected:
	xJobQueue NewDnsJobQueue;
	xJobList  FinishedDnsJobList;
	xSpinlock FinishedDnsJobListLock;

	xRunState                RunState;
	std::vector<std::thread> WorkerThreads;
	xNotifierFunc *          Notifier    = nullptr;
	xVariable                NotifierCtx = {};
	int_fast32_t             MaxDnsJob   = 1'0000;
	std::atomic_int_fast32_t TotalDnsJob = 0;

protected:
	static void IgnoreResult(xVariable CallbackCtx, xVariable RequestCtx, const xNetAddress & A4, const xNetAddress & A6) {};
};
