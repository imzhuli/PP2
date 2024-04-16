#include "./audit.hpp"

xProxyAudit ProxyAudit;

static uint64_t LastOutputMS = 0;

void OutputAudit(uint64_t NowMS) {
	auto Diff = NowMS - LastOutputMS;
	if (Diff < 600'000) {
		return;
	}
	LastOutputMS = NowMS;

	cout << "Audit DiffMS=" << (Diff / 1000) << endl;
	cout << ": AuthCacheSize=" << ProxyAudit.AuthCacheSize;
	cout << ", ClientConnectionCount=" << ProxyAudit.ClientConnectionCount;
	//
	cout << ", AuthCacheOverflow=" << Steal(ProxyAudit.AuthCacheOverflow);
	cout << ", AuthCacheFailure=" << Steal(ProxyAudit.AuthCacheFailure);
	cout << endl;
}
