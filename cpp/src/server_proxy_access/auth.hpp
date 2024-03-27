#pragma once
#include "../common/base.hpp"

enum xProxyAuthState : int32_t {
	PAS_INIT     = 0,
	PAS_QUERYING = 1,
	PAS_BANNED   = 2,
};

struct xProxyAuthRequestNode : xListNode {};

struct xProxyAuthNode final : xListNode {
	uint64_t        LocalUserIndex   = 0;
	uint64_t        GlobalUserId     = 0;
	xProxyAuthState RemoteAuthState  = PAS_INIT;
	ubyte           PassworkHash[16] = {};

	uint64_t ActiveConnectionCount = 0;
	uint64_t LocalUpdateSize       = 0;
	uint64_t LocalDownloadSize     = 0;

	xList<xProxyAuthRequestNode> RequestList = {};
};

extern bool InitProxyAuth();
extern void CleanProxyAuth();
extern void TickProxyAuth();

extern bool PostAuthRequest(xProxyAuthRequestNode & RequestNode, const std::string & User, const std::string & Password);
