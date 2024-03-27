#include "./audit.hpp"

#include <iostream>
using namespace std;

static constexpr const uint64_t OutputTickerTimeout = 60'000;

xAudit Audit;

static auto AuditTicker   = xTicker();
static auto LastAuditTick = (uint64_t)AuditTicker;

void OutputAudit() {
	AuditTicker.Update();
	if (AuditTicker - LastAuditTick < OutputTickerTimeout) {
		return;
	}
	LastAuditTick = AuditTicker;

	cout << "Audit";
	cout << ", DnsQueryCounter=" << Audit.DnsQueryCounter;
	cout << ", DnsRequestCounter=" << Audit.DnsRequestCounter;
	cout << ", TotalDnsJob=" << Audit.TotalDnsJob;
	cout << endl;
}
