#include "./relay_object_manager.hpp"

xel::xIndexedStorage<xRelayAbstractDevice *>     RelayDeviceManager;
xel::xIndexedStorage<xRelayAbstractConnection *> RelayConnectionManager;
xel::xIndexedStorage<xRelayAbstractUdpChannel *> RelayUdpChannelManagerl;

static auto RelayDeviceManagerGuard      = xResourceGuard(RelayDeviceManager, xel::xObjectIdManager::MaxObjectId);
static auto RelayConnectionManagerGuard  = xResourceGuard(RelayConnectionManager, xel::xObjectIdManager::MaxObjectId);
static auto RelayUdpChannelManagerlGuard = xResourceGuard(RelayUdpChannelManagerl, xel::xObjectIdManager::MaxObjectId);
