#pragma once
#include "./device_abstract.hpp"
#include "./pa_future.hpp"
#include "./relay_abstract.hpp"

struct xRelayLocalDevice {
    uint64_t    DeviceId;
    xNetAddress BindAddress;
    xNetAddress ExportAddress;
};

struct xRelayLocalDeviceConnectionTimeoutNode : xListNode {
    uint64_t TimestampMS = 0;
};
using xRelayLocalDeviceConnectionTimeoutList = xList<xRelayLocalDeviceConnectionTimeoutNode>;

struct xRelayLocalDeviceConnection final
    : xRelayLocalDeviceConnectionTimeoutNode
    , xTcpConnection {
    uint64_t      DeviceId     = 0;
    uint64_t      ConnectionId = 0;
    uint64_t      PASideConnectionId;
    xFutureHandle CreateConnectionFutureHandle;
    bool          DeleteMark = false;
};

struct xRelayLocalDeviceUdpChannelTimeoutNode : xListNode {
    uint64_t TimestampMS = 0;
};
using xRelayLocalDeviceUdpChannelTimeoutList = xList<xRelayLocalDeviceUdpChannelTimeoutNode>;

struct xRelayLocalDeviceUdpChannel final
    : xRelayLocalDeviceUdpChannelTimeoutNode
    , xUdpChannel {
    uint64_t DeviceId   = 0;
    uint64_t ChannelId  = 0;
    bool     DeleteMark = false;
};

class xRelayLocalBindingService final
    : xRelayAbstractService
    , xTcpConnection::iListener {

public:
    bool Init(const std::vector<std::pair<xNetAddress, xNetAddress>> & BindAddressPairList);
    void Clean();

    void CreateConnection(uint64_t DeviceId, uint64_t PASideConnectionId, const xNetAddress & TargetAddress, xRelayCreateConnectionFuture & Future) override;
    void DeferDestroyConnection(uint64_t ConnectionId) override;
    void PostData(uint64_t ConnectionId, const void * Payload, size_t PayloadSize) override;
    void PostData(uint64_t UdpChannelId, const xel::xNetAddress & TargetAddress, const void * Payload, size_t PayloadSize) override;

private:
    bool CreateLocalDeviceList(const std::vector<std::pair<xNetAddress, xNetAddress>> & BindAddressPairList);
    void DestroyLocalDeviceList();
    auto GetDevice(uint64_t DeviceId) const -> const xRelayLocalDevice *;
    void KeepAlive(xRelayLocalDeviceConnection * Connection);
    void DeferDestroyConnection(xRelayLocalDeviceConnection * Connection);
    void DestroyAllConnections();
    void DestroyAllUdpChannels();

private:  // listener
    void   OnConnected(xTcpConnection * TcpConnectionPtr) override;
    void   OnPeerClose(xTcpConnection * TcpConnectionPtr) override;
    void   OnFlush(xTcpConnection * TcpConnectionPtr) override {}
    size_t OnData(xTcpConnection * TcpConnectionPtr, ubyte * DataPtr, size_t DataSize) override {
        return DataSize;
    }

private:
    using xLocalDeviceList = std::vector<xRelayLocalDevice>;

    xTicker LocalTicker;

    xLocalDeviceList                                  LocalDeviceList;
    xel::xIndexedStorage<xRelayLocalDeviceConnection> LocalConnectionPool;
    xel::xIndexedStorage<xRelayLocalDeviceUdpChannel> LocalUdpChannelPool;

    xRelayLocalDeviceConnectionTimeoutList ConnectionEstablishTimeoutList;
    xRelayLocalDeviceConnectionTimeoutList ConnectionIdleTimeoutList;
    xRelayLocalDeviceConnectionTimeoutList ConnectionIdleKillList;

    xRelayLocalDeviceUdpChannelTimeoutList UdpChannelTimeoutList;
    xRelayLocalDeviceUdpChannelTimeoutList UdpChannelKillList;
};
