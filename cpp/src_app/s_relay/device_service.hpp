#pragma once
#include <pp_common/_common.hpp>

constexpr const size_t MAX_DEVICE_CONNECTION_ON_RELAY = 20'0000;

void InitDeviceService(const xNetAddress & BindAddress);
void CleanDeviceService();
void DeviceTicker(uint64_t NowMS);

void UpdateDeviceStateRelayServerList(const std::vector<xServerInfo> & ServerList);
