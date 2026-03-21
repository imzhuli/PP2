#pragma once
#include "./relay_abstract.hpp"

class xRelayLocalBindingConnection;
class xRelayLocalBindingDevice;

struct xRelayLocalBindingConnection final : xel::xTcpConnection {
    
    xRelayLocalBindingDevice * Owner = nullptr;
};

class xRelayLocalBindingDevice final : public xRelayAbstractDevice {
public:
    bool Init(const xNetAddress & LocalDevice);
    void Clean();

    uint64_t CreateConnection(const std::string & TargetHostname, uint16_t Port) override;
    uint64_t CreateConnection(const xel::xNetAddress & TargetAddress) override;
    void     DestroyConnection(uint64_t ConnectionId) override;
    uint64_t CreateUdpChannel() override;
    void     DestroyUdpChannel(uint64_t ChannelId) override;

private:
    void DestroyAllConnections();
    void DestroyAllUdpChannels();

private:
    xNetAddress BindAddress;
};
