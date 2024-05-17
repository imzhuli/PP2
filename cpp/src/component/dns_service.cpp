#include "./dns_service.hpp"

#include <atomic>

#if defined(X_SYSTEM_LINUX) || defined(X_SYSTEM_DARWIN)
#include <arpa/inet.h>
#include <netdb.h>
#endif

bool xDnsService::Init(xNotifierFunc & WakeupFunc, xVariable WakeupCtx) {
	assert(WakeupFunc);
	Notifier    = WakeupFunc;
	NotifierCtx = WakeupCtx;
	RunState.Start();
	for (int i = 0; i < 50; ++i) {
		WorkerThreads.push_back(std::thread([this] { WorkerThreadFunc(); }));
	}
	return true;
}

void xDnsService::Clean() {
	RunState.Stop();
	NewDnsJobQueue.PostWakeupN(WorkerThreads.size());
	for (auto & T : WorkerThreads) {
		T.join();
	}
	GetAndDispatchResult(IgnoreResult, {});
	Reset(Notifier);
	RunState.Finish();
	// post check
	assert(!TotalDnsJob);
}

bool xDnsService::Query(const std::string & Hostname, xVariable JobCtx) {
	auto JP = NewDnsJob(Hostname, JobCtx);
	if (!JP) {
		return false;
	}
	NewDnsJobQueue.PostJob(*JP);
	return true;
}

void xDnsService::GetAndDispatchResult(xDnsResultCallback Callback, xVariable CallbackCtx) {
	xJobList JL = {};
	do {
		auto G = xSpinlockGuard(FinishedDnsJobListLock);
		JL.GrabListTail(FinishedDnsJobList);
	} while (false);
	while (auto JP = static_cast<xDnsJob *>(JL.PopHead())) {
		Callback(CallbackCtx, JP->JobCtx, JP->A4, JP->A6);
		DeleteDnsJob(JP);
	}
}

xDnsJob * xDnsService::NewDnsJob(const std::string & Hostname, xVariable JobCtx) {
	if (++TotalDnsJob >= MaxDnsJob) {
		--TotalDnsJob;
		return nullptr;
	}
	auto PJ      = new xDnsJob;
	PJ->Hostname = Hostname;
	PJ->JobCtx   = JobCtx;
	return PJ;
}

void xDnsService::DeleteDnsJob(const xDnsJob * PJ) {
	delete PJ;
	--TotalDnsJob;
}

void xDnsService::WorkerThreadFunc() {
	while (auto PJ = (xDnsJob *)NewDnsJobQueue.WaitForJob()) {

		addrinfo   hints = {};
		addrinfo * res   = {};
		addrinfo * p     = {};

		memset(&hints, 0, sizeof(hints));
		hints.ai_family   = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;

		X_DEBUG_BREAKPOINT();
		if (auto err = getaddrinfo(PJ->Hostname.c_str(), nullptr, &hints, &res)) {
			Touch(err);
			X_DEBUG_PRINTF("getaddrinfo: %s\n", gai_strerror(err));
		} else {
			for (p = res; p != NULL; p = p->ai_next) {
				if (p->ai_family == AF_INET) {  // IPv4
					if (PJ->A4) {
						continue;
					}
					PJ->A4 = xNetAddress::Parse((sockaddr_in *)p->ai_addr);
				} else if (p->ai_family == AF_INET6) {
					if (PJ->A6) {
						continue;
					}
					PJ->A6 = xNetAddress::Parse((sockaddr_in6 *)p->ai_addr);
				}
			}
			freeaddrinfo(res);
		}

		// finished:
		do {
			auto G = xSpinlockGuard(FinishedDnsJobListLock);
			FinishedDnsJobList.GrabTail(*PJ);
		} while (false);
		(*Notifier)(NotifierCtx);
	}
}
