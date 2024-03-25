#pragma once
#include "../common/base.hpp"

#include <atomic>

struct xAudit {
	std::atomic_size_t DnsQueryCounter   = 0;  // query from clients
	std::atomic_size_t DnsRequestCounter = 0;  // request against dns server
	std::atomic_size_t TotalDnsJob       = 0;
};

extern xAudit Audit;
extern void   OutputAudit();
