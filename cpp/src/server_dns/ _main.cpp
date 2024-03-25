#include "../common/base.hpp"
#include "../common_job/job.hpp"
#include "./audit.hpp"
#include "./dns_job.hpp"

static constexpr uint64_t DnsCacheTimeoutMS = 15 * 60'000;

/**************/
static xRunState RunState;

int main(int argc, char ** argv) {
	RunState.Start();

	std::array<std::thread, 50> Threads;
	for (auto & T : Threads) {
		T = std::thread(DnsQueryThread);
	}

	if (auto TJ = NewDnsJob("www.baidu.com")) {
		NewDnsJobQueue.PostJob(*TJ);
	}

	xJobList FJL = {};
	while (true) {

		// finished job list
		do {
			auto G = xSpinlockGuard(FinishedJobListLock);
			FJL.GrabListTail(FinishedJobList);
		} while (false);
		for (auto & Node : FJL) {
			auto & J = static_cast<xDnsJob &>(Node);
			X_DEBUG_PRINTF("%s:%s", J.Hostname.c_str(), J.A4.IpToString().c_str());
			// TODO: post result
			DeleteDnsJob(&J);
		}
		// end of finished job list

		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		OutputAudit();
	}

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

	RunState.Finish();
	return 0;
}
