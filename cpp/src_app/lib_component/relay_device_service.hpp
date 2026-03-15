#pragma once
#include "./relay_abstract.hpp"

extern void EnableRelayRemoteDeviceService(const xel::xNetAddress & BindAddress);
extern void DisableRelayRemoteDeviceService();

extern std::function<void()> RelayOnRemoteDeviceEnabled;
extern std::function<void()> RelayOnRemoteDeviceKeepAlive;
extern std::function<void()> RelayOnRemoteDeviceRemoved;
