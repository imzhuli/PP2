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
    enum eType {
        UNDETERMINED = 0,
        S5_CHALLENGE,
        S5_TCP,
        S5_UDP,
        HTTP_CHALLENGE,
        HTTP_RAW,
        HTTP_1,
    };

    uint64_t               ConnectionId   = 0;
    xPA_ClientUdpChannel * BindUdpChannel = {};
    bool                   DeleteMark     = false;
    xPA_TcpDataProcessor   DataProcessor  = {};
    eType                  Type           = UNDETERMINED;
    xPA_AuthFuture *       AuthFuture     = nullptr;
};

struct xPA_ClientUdpChannel
    : public xUdpChannel {
    uint64_t               UdpChannelId     = 0;
    xPA_ClientConnection * ParentConnection = 0;
    xPA_UdpDataProcessor   DataProcessor    = {};
};

class xProxyAccessService final
    : xTcpServer::iListener
    , xTcpConnection::iListener
    , xUdpChannel::iListener {
public:
    bool Init(const xNetAddress & BindAddress4, const xNetAddress & BindAddress6);
    void Clean();
    void Tick(uint64_t NowMS);
    void BindAuthService(xAuthAbstractService * Service) { AuthService = Service; }
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
    void   OutputAudit();
    size_t KillConnectionOnData(xPA_ClientConnection * Connection, ubyte * DataPtr, size_t DataSize);
    size_t OnGuessProxyType(xPA_ClientConnection * Connection, ubyte * DataPtr, size_t DataSize);
    size_t OnS5Challenge(xPA_ClientConnection * Connection, ubyte * DataPtr, size_t DataSize);
    size_t OnS5AuthInfo(xPA_ClientConnection * Connection, ubyte * DataPtr, size_t DataSize);
    size_t OnHttpChallenge(xPA_ClientConnection * Connection, ubyte * DataPtr, size_t DataSize);

    xPA_AuthFuture * RequestAuthentication(xPA_ClientConnection * Connection, std::string_view AuthView);
    void             OnAuthResult(xPA_AuthFuture * Future);
    void             OnS5AuthResult(xPA_ClientConnection * Connection);

protected:
    void KeepAlive(xPA_ClientConnection * Connection);
    void DeferKill(xPA_ClientConnection * Connection);
    void CheckAndReleaseAuthFuture(xPA_ClientConnection * Connection);
    void ReleaseAuthFuture(xPA_ClientConnection * Connection);
    void DestroyConnection(xPA_ClientConnection * Connection);
    void ScheduleAuthTimeoutConnection();
    void ExcuteKillConnection();
    void ClearTimeoutFuture();

private:
    xTicker      LocalTicker;
    xTcpServer * TcpServer4 = nullptr;
    xTcpServer * TcpServer6 = nullptr;

    xIndexedStorage<xPA_ClientConnection> ClientConnectionPool;
    xIndexedStorage<xPA_ClientUdpChannel> ClientUdpChannelPool;
    xPA_ClientConnectionTimeoutList       ClientConnectionTimeoutList;
    xPA_ClientConnectionTimeoutList       ClientConnectionAuthTimeoutList;
    xPA_ClientConnectionTimeoutList       ClientConnectionKillList;

    xFuturePoolManager<xPA_AuthFuture>                    AuthFutureManager;
    xFuturePoolManager<xPA_AcquireDeviceFuture>           AcquireDeviceFutureManager;
    xFuturePoolManager<xPA_AcquireDeviceConnectionFuture> AcquireDeviceConnectionFutureManager;
    xFuturePoolManager<xPA_AcquireDeviceUdpChannelFuture> AcquireDeviceUdpChannelFutureManager;

    xAuthAbstractService *   AuthService   = nullptr;
    xDeviceAbstractService * DeviceService = nullptr;

    xNetAddress BindUdpAddress4   = {};
    xNetAddress BindUdpAddress6   = {};
    xNetAddress ExportUdpAddress4 = {};
    xNetAddress ExportUdpAddress6 = {};

    xFutureList AuthFutureTimeoutList;
    xFutureList AcquireDeviceConnectionFutureTimeoutList;
    xFutureList AcquireDeviceUdpChannelFutureTimeoutList;

    struct xAudit {
        size_t InvalidS5AuthTypeCount   = 0;
        size_t InvalidS5AuthInfo        = 0;
        size_t InvalidS5AuthResult      = 0;
        size_t RequestAuthenticationOOM = 0;
        size_t AuthTimeoutCount         = 0;

        uint64_t LastOutputTimestampMS = GetTimestampMS();
    } Audit;
};
