#pragma once
#include <pp_common/_common.hpp>

void InitRelayService(const xNetAddress & InputAddress, const xNetAddress & OutputAddress);
void CleanRelayService();
void RelayTicker(uint64_t NowMS);