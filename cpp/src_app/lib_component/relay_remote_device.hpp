#pragma once
#include "./relay_abstract.hpp"

struct xRelayRemoteDevice final : xRelayAbstractDevice {
    uint64_t CreateConnection(const std::string & TargetHostname, uint16_t Port) override;
    uint64_t CreateConnection(const xel::xNetAddress & TargetAddress) override;
    void     DestroyConnection(uint64_t ConnectionId) override;
    uint64_t CreateUdpChannel() override;
    void     DestroyUdpChannel(uint64_t ChannelId) override;
};