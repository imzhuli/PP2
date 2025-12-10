#pragma once
#include <pp_common/service_runtime.hpp>

void InitRelayPort();
void CleanRelayPort();
void RelayPortTick(uint64_t NowMS);
