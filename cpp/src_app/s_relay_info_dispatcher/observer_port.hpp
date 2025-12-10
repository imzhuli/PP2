#pragma once
#include <pp_common/service_runtime.hpp>

struct xRID_ObserverContext : xListNode {
    uint64_t StartTimestampMS = 0;
};

void InitObserverPort();
void CleanObserverPort();
void ObserverPortTick(uint64_t NowMS);
