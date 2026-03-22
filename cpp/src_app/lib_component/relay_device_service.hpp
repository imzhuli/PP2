#pragma once
#include "./relay_abstract.hpp"
#include "./relay_local_binding_device.hpp"
#include "./relay_remote_device.hpp"

extern xel::xIndexedStorage<xRelayAbstractDevice *> DeviceIdManager;

extern void EnableRelayRemoteDeviceService(const xel::xNetAddress & BindAddress);
extern void DisableRelayRemoteDeviceService();

extern std::function<void()> RelayOnRemoteDeviceEnabled;
extern std::function<void()> RelayOnRemoteDeviceKeepAlive;
extern std::function<void()> RelayOnRemoteDeviceRemoved;

extern void EnableRelayLocalBindingDeviceService(const char * LocalBindingFilename);
extern void DisableRelayLocaBindinglDeviceService();
