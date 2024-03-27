#pragma once
#include "../common/base.hpp"

struct xProxyAudit {
	size_t AuthCacheSize = 0;

	// cleared periodically
	size_t AuthCacheOverflow = 0;
	size_t AuthCacheFailure  = 0;  // 考虑到有攻击的可能性, 仅作为参考
};

extern xProxyAudit ProxyAudit;
extern void        OutputAudit(uint64_t NowMS);
