#pragma once
#include <pp_common/_common.hpp>

void InitDeviceService(const xNetAddress & BindAddress);
void CleanDeviceService();
void DeviceTicker(uint64_t NowMS);

void UpdateDeviceStateRelayServerList(const std::vector<xServiceInfo> & ServerList);
void UpdateRelayInfoDispatcherServerList(const std::vector<xServiceInfo> & ServerList);
