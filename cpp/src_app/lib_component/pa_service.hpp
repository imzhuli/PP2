#pragma once
#include "./device_abstract.hpp"
#include "./pa_abstract.hpp"
#include "./pa_future.hpp"

class xProxyAccessService;
using xPA_TcpDataProcessor = size_t (xProxyAccessService::*)(xPA_ClientConnection * Connection, ubyte * DataPtr, size_t DataSize);
using xPA_UdpDataProcessor = size_t (xProxyAccessService::*)(xPA_ClientConnection * Connection, ubyte * DataPtr, size_t DataSize);

struct xPA_ClientConnectionTimeoutNode : xListNode {
    uint64_t TimestampMS = 0;
};
using xPA_ClientConnectionTimeoutList = xList<xPA_ClientConnectionTimeoutNode>;

struct xPA_ClientConnection
    : public xTcpConnection
    , public xPA_ClientConnectionTimeoutNode {
    uint64_t             ConnectionId     = 0;
    uint64_t             BindUdpChannelId = 0;
    bool                 DeleteMark       = false;
    xPA_TcpDataProcessor DataProcessor    = {};
};

struct xPA_ClientUdpChannel
    : public xUdpChannel {
    uint64_t             BindConnectionId = 0;
    xPA_UdpDataProcessor DataProcessor    = {};
};

class xProxyAccessService final
    : xTcpServer::iListener
    , xTcpConnection::iListener
    , xUdpChannel::iListener {
public:
    bool Init(const xNetAddress & BindAddress4, const xNetAddress & BindAddress6);
    void Clean();
    void Tick(uint64_t NowMS);
    void BindDeviceService(xDeviceAbstractService * Service) { DeviceService = Service; }
    void EnableUdp4(const xNetAddress & BindAddress, const xNetAddress & ExportAddress);
    void EnableUdp6(const xNetAddress & BindAddress, const xNetAddress & ExportAddress);

protected:  // override:
    void   OnNewConnection(xTcpServer * TcpServerPtr, xSocket && NativeHandle) override;
    void   OnConnected(xTcpConnection * TcpConnectionPtr) override;
    void   OnPeerClose(xTcpConnection * TcpConnectionPtr) override;
    void   OnFlush(xTcpConnection * TcpConnectionPtr) override;
    size_t OnData(xTcpConnection * TcpConnectionPtr, ubyte * DataPtr, size_t DataSize) override;
    void   OnData(xUdpChannel * UdpChannelPtr, ubyte * DataPtr, size_t DataSize, const xNetAddress & RemoteAddress) override;

protected:  // process data:
    size_t OnGuessProxyType(xPA_ClientConnection * Connection, ubyte * DataPtr, size_t DataSize);

protected:
    void KeepAlive(xPA_ClientConnection * Connection);
    void DeferKill(xPA_ClientConnection * Connection);
    void DestroyConnection(xPA_ClientConnection * Connection);
    void ClearTimeoutFuture();

private:
    xTicker      LocalTicker;
    xTcpServer * TcpServer4 = nullptr;
    xTcpServer * TcpServer6 = nullptr;

    xIndexedStorage<xPA_ClientConnection> ClientConnectionPool;
    xIndexedStorage<xPA_ClientUdpChannel> ClientUdpChannelPool;
    xPA_ClientConnectionTimeoutList       ClientConnectionTimeoutList;
    xPA_ClientConnectionTimeoutList       ClientConnectionKillList;

    xFuturePoolManager<xPA_AuthFuture>                   AuthFutureManager;
    xFuturePoolManager<xPA_AcquireDeviceFuture>          AcquireDeviceFutureManager;
    xFuturePoolManager<xPA_CreateDeviceConnectionFuture> CreateDeviceConnectionFutureManager;
    xFuturePoolManager<xPA_CreateDeviceUdpChannelFuture> CreateDeviceUdpChannelFutureManager;

    xDeviceAbstractService * DeviceService;

    xNetAddress BindUdpAddress4   = {};
    xNetAddress BindUdpAddress6   = {};
    xNetAddress ExportUdpAddress4 = {};
    xNetAddress ExportUdpAddress6 = {};

    xFutureList AuthFutureTimeoutList;
    xFutureList CreateDeviceConnectionFutureTimeoutList;
    xFutureList CreateDeviceUdpChannelFutureTimeoutList;
};
