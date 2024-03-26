#include "./dns_job.hpp"

#include "./audit.hpp"

#include <atomic>

#if defined(X_SYSTEM_LINUX) || defined(X_SYSTEM_DARWIN)
#include <arpa/inet.h>
#include <netdb.h>
#endif

static constexpr const int_fast32_t MAX_DNS_JOB = 10'000;
static std::atomic_int_fast32_t     TotalDnsJob = 0;

xJobQueue NewDnsJobQueue;
xJobList  FinishedJobList;
xSpinlock FinishedJobListLock;

xDnsJob * NewDnsJob(const std::string & Hostname, xVariable JobCtx) {
	if (++TotalDnsJob >= MAX_DNS_JOB) {
		--TotalDnsJob;
		return nullptr;
	}
	auto PJ      = new xDnsJob;
	PJ->Hostname = Hostname;
	PJ->JobCtx   = JobCtx;

	Audit.TotalDnsJob = TotalDnsJob;
	return PJ;
}

void DeleteDnsJob(const xDnsJob * PJ) {
	assert(PJ);
	delete PJ;
	--TotalDnsJob;

	Audit.TotalDnsJob = TotalDnsJob;
}

void DnsWorkerThread(xNotifier Notifier, xVariable Ctx) {
	while (auto PJ = (xDnsJob *)NewDnsJobQueue.WaitForJob()) {

		addrinfo   hints = {};
		addrinfo * res   = {};
		addrinfo * p     = {};

		memset(&hints, 0, sizeof(hints));
		hints.ai_family   = AF_INET;
		hints.ai_socktype = SOCK_STREAM;

		X_DEBUG_BREAKPOINT();
		if (auto status = getaddrinfo(PJ->Hostname.c_str(), nullptr, &hints, &res)) {
			X_DEBUG_PRINTF("getaddrinfo: %s\n", gai_strerror(status));
		} else {
			for (p = res; p != NULL; p = p->ai_next) {
				if (p->ai_family == AF_INET) {  // IPv4
					PJ->A4 = xNetAddress::Parse((sockaddr_in *)p->ai_addr);
					break;
				}
				// else if (p->ai_family == AF_INET) {
				// 	PJ->A6 = xNetAddress::Parse((sockaddr_in6 *)p->ai_addr);
				// }
			}
			freeaddrinfo(res);
		}

		// finished:
		do {
			auto G = xSpinlockGuard(FinishedJobListLock);
			FinishedJobList.GrabTail(*PJ);
		} while (false);
		Notifier(Ctx);
	}
}
