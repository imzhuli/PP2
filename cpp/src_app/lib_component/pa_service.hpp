#pragma once
#include "./device_abstract.hpp"
#include "./pa_abstract.hpp"
#include "./pa_future.hpp"

class xPA_ClientConnectionTimeoutNode : xListNode {
    uint64_t TimestampMS = 0;
};
using xPA_ClientConnectionTimeoutList = xList<xPA_ClientConnectionTimeoutNode>;

class xPA_ClientConnection : xTcpConnection {
};

class xPA_CreateRelayTcpConnectionFutureManager : public xFutureManager {
};

class xPA_Service
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
    void ClearTimeoutFuture();

    virtual void ReleaseAuthFuture(uint64_t FutureId);
    virtual void ReleaseCreateRelayTcpConnectionFuture(uint64_t FutureId);

private:
    xTicker                                    LocalTicker;
    xTcpServer                                 TcpServer;
    xel::xIndexedStorage<xPA_ClientConnection> ClientConnectionPool;

    xFutureManagerBase<xPA_AuthFuture>                   AuthFutureManager;
    xFutureManagerBase<xPA_AcquireDeviceFuture>          AcquireDeviceFutureManager;
    xFutureManagerBase<xPA_CreateDeviceConnectionFuture> CreateDeviceConnectionFutureManager;
    xFutureManagerBase<xPA_CreateDeviceUdpChannelFuture> CreateDeviceUdpChannelFutureManager;

    xDeviceAbstractService * DeviceService;

    xPA_CreateRelayTcpConnectionFutureManager * CreateRelayTcpConnectionFutureManager;
    xPA_CreateRelayTcpConnectionFutureManager * CreateRelayUdpChannelFutureManager;

    xFutureList AuthFutureTimeoutList;
    xFutureList CreateDeviceConnectionFutureTimeoutList;
    xFutureList CreateDeviceUdpChannelFutureTimeoutList;
};
