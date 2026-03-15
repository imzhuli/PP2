#pragma once
#include "./relay_abstract.hpp"

#include <pp_common/_common.hpp>

extern xel::xIndexedStorage<xRelayAbstractDevice *>     RelayDeviceManager;
extern xel::xIndexedStorage<xRelayAbstractConnection *> RelayConnectionManager;
extern xel::xIndexedStorage<xRelayAbstractUdpChannel *> RelayUdpChannelManagerl;