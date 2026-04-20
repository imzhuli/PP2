#pragma once
#include "./device_abstract.hpp"
#include "./pa_abstract.hpp"
#include "./pa_future.hpp"

struct xPA_ClientConnectionTimeoutNode : xListNode {
    uint64_t TimestampMS = 0;
};
using xPA_ClientConnectionTimeoutList = xList<xPA_ClientConnectionTimeoutNode>;

struct xPA_ClientConnection
    : public xTcpConnection
    , public xPA_ClientConnectionTimeoutNode {
    uint64_t ConnectionId = 0;
    bool     DeleteMark   = false;
};

class xProxyAccessService final
    : xTcpServer::iListener
    , xTcpConnection::iListener
    , xUdpChannel::iListener {
public:
    bool Init(const xNetAddress & BindAddress);
    void Clean();
    void Tick(uint64_t NowMS);
    void BindDeviceService(xDeviceAbstractService * Service) { DeviceService = Service; }

protected:  // override:
    void   OnNewConnection(xTcpServer * TcpServerPtr, xSocket && NativeHandle) override;
    void   OnConnected(xTcpConnection * TcpConnectionPtr) override;
    void   OnPeerClose(xTcpConnection * TcpConnectionPtr) override;
    void   OnFlush(xTcpConnection * TcpConnectionPtr) override;
    size_t OnData(xTcpConnection * TcpConnectionPtr, ubyte * DataPtr, size_t DataSize) override;
    void   OnData(xUdpChannel * ChannelPtr, ubyte * DataPtr, size_t DataSize, const xNetAddress & RemoteAddress) override;

protected:
    void KeepAlive(xPA_ClientConnection * Connection);
    void DeferKill(xPA_ClientConnection * Connection);
    void DestroyConnection(xPA_ClientConnection * Connection);
    void ClearTimeoutFuture();

private:
    xTicker                               LocalTicker;
    xTcpServer                            TcpServer;
    xIndexedStorage<xPA_ClientConnection> ClientConnectionPool;
    xPA_ClientConnectionTimeoutList       ClientConnectionTimeoutList;
    xPA_ClientConnectionTimeoutList       ClientConnectionKillList;

    xFuturePoolManager<xPA_AuthFuture>                   AuthFutureManager;
    xFuturePoolManager<xPA_AcquireDeviceFuture>          AcquireDeviceFutureManager;
    xFuturePoolManager<xPA_CreateDeviceConnectionFuture> CreateDeviceConnectionFutureManager;
    xFuturePoolManager<xPA_CreateDeviceUdpChannelFuture> CreateDeviceUdpChannelFutureManager;

    xDeviceAbstractService * DeviceService;

    xFutureList AuthFutureTimeoutList;
    xFutureList CreateDeviceConnectionFutureTimeoutList;
    xFutureList CreateDeviceUdpChannelFutureTimeoutList;
};
