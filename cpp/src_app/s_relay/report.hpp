#pragma once
#include <pp_common/service_runtime.hpp>

void InitRelayReport();
void CleanRelayReport();
void TickRelayReport(uint64_t NowMS);

void UpdateRelayInfoDispatcherServerList(const std::vector<xServerInfo> & ServerList);