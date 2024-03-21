#pragma once
#include "./base.hpp"

#include <core/list.hpp>

struct xRequestContext : xListNode {
	uint64_t TimestampMS;
	xIndexId RequestId;
};

using xRequestContxtList = xList<xRequestContext>;
