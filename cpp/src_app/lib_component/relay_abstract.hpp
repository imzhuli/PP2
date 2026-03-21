#pragma once
#include <pp_common/_common.hpp>
#include <pp_common/device.hpp>

struct xRelayAbstractDevice;
struct xRelayAbstractConnection;
struct xRelayAbstractUdpChannel;

struct xRelayLifeCycleNode : xListNode {};

struct xRelayAbstractConnection : xRelayLifeCycleNode{
    xRelayAbstractDevice * Owner = nullptr;

    virtual bool IsOpen() const                                                                             = 0;
    virtual bool IsConnected() const                                                                        = 0;
    virtual bool IsClosing() const                                                                          = 0;
    virtual bool IsClosed() const                                                                           = 0;
    virtual void PostData(const xel::xNetAddress & TargetAddress, const void * Payload, size_t PayloadSize) = 0;

    virtual void SetUploadSpeedLimit(size_t Limit) {};
    virtual void SetDownloadSpeedLimit(size_t Limit) {};
};

struct xRelayAbstractUdpChannel : xRelayLifeCycleNode {
    xRelayAbstractDevice * Owner = nullptr;

    virtual void PostData(const xel::xNetAddress & TargetAddress, const void * Payload, size_t PayloadSize) = 0;
};

struct xRelayAbstractDevice {
    uint64_t DeviceId = 0;

    virtual uint64_t CreateConnection(const std::string & TargetHostname, uint16_t Port) = 0;
    virtual uint64_t CreateConnection(const xel::xNetAddress & TargetAddress)            = 0;
    virtual void     DestroyConnection(uint64_t ConnectionId)                            = 0;
    virtual uint64_t CreateUdpChannel()                                                  = 0;
    virtual void     DestroyUdpChannel(uint64_t ChannelId)                               = 0;
};

