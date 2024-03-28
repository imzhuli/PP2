#pragma once
#include "../common/base.hpp"

struct xDnsReq : xListNode {
	uint64_t  Index;
	uint64_t  Timestamp;
	xVariable Context;
};

// struct xDnsCacheNode : xListNode {
// 	enum eState {
// 		INIT        = 0,
// 		FIRST_QUERY = 1,
// 		UPDATING    = 2,
// 		UNAVAILABLE = 3,
// 	};
// 	eState         State;
// 	uint64_t       TimestampMS;
// 	xNetAddress    A4;
// 	xNetAddress    A6;
// 	xList<xDnsReq> RequestList;
// };

using xDnsResultCallback = void(xVariable Ctx, const xNetAddress & A4, const xNetAddress & A6);

extern void InitDnsCache(xNotifier Notifier, xVariable Ctx = {}, bool EnableLocal = false);
extern void CleanDnsCache();

extern bool PostDnsRequest(const std::string & Hostname, xVariable Ctx);
extern void DispatchResults(xDnsResultCallback Callback);
extern void ShrinkDnsCache();
