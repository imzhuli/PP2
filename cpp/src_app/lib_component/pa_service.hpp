#pragma once
#include "./abstract/audit_abstract.hpp"
#include "./abstract/device_abstract.hpp"
#include "./abstract/pa_abstract.hpp"
#include "./abstract/relay_abstract.hpp"
#include "./pa_future.hpp"

struct xPA_ClientConnection;
struct xPA_ClientUdpChannel;
class xProxyAccessService;

using xPA_TcpDataProcessor = size_t (xProxyAccessService::*)(xPA_ClientConnection * Connection, ubyte * DataPtr, size_t DataSize);
using xPA_UdpDataProcessor = size_t (xProxyAccessService::*)(xPA_ClientConnection * Connection, ubyte * DataPtr, size_t DataSize);

struct xPA_ClientUdpChannel
    : private xUdpChannel {
    uint64_t               UdpChannelId          = 0;
    xPA_ClientConnection * ParentConnection      = 0;
    uint64_t               RelaySideUdpChannelId = {};
    xNetAddress            ClientIpCheckAddress  = {};
    xNetAddress            LastClientIpAddress   = {};

    using xUdpChannel::Clean;
    using xUdpChannel::GetBindAddress;
    using xUdpChannel::Init;

    friend struct xPA_ClientConnection;
    static xPA_ClientUdpChannel * From(xUdpChannel * UdpChannel) {
        return static_cast<xPA_ClientUdpChannel *>(UdpChannel);
    }
};

struct xPA_ClientConnectionTimeoutNode : xListNode {
    uint64_t TimestampMS = 0;
};
using xPA_ClientConnectionTimeoutList = xList<xPA_ClientConnectionTimeoutNode>;
struct xPA_ClientConnectionUpdateConsumedDataNode : xListNode {
    size_t LastReportedConsumedSize = 0;
};
using xPA_ClientConnectionUpdateConsumedDataList = xList<xPA_ClientConnectionUpdateConsumedDataNode>;

struct xPA_ClientConnection
    : private xTcpConnection
    , public xPA_ClientConnectionTimeoutNode
    , public xPA_ClientConnectionUpdateConsumedDataNode {
    enum eType {
        UNDETERMINED = 0,
        S5_CHALLENGE,
        S5_TCP,
        S5_UDP,
        HTTP_RAW,
        HTTP_NORMAL,
    };

    uint64_t                            ConnectionId                  = 0;
    xNetAddress                         ExportIp                      = {};
    xPA_ClientUdpChannel *              BindUdpChannel                = {};
    bool                                DeleteMark                    = false;
    bool                                NoAuth                        = false;
    xPA_TcpDataProcessor                DataProcessor                 = {};
    eType                               Type                          = UNDETERMINED;
    xPA_AuthFuture *                    AuthFuture                    = nullptr;
    xPA_AcquireDeviceFuture *           AcquireDeviceFuture           = nullptr;
    xPA_AcquireDeviceConnectionFuture * AcquireDeviceConnectionFuture = nullptr;
    xPA_AcquireDeviceUdpChannelFuture * AcquireDeviceUdpChannelFuture = nullptr;
    uint64_t                            LocalAuthId                   = {};
    uint64_t                            GlobalAuthId                  = {};
    xDeviceReference                    DeviceReference               = {};
    uint64_t                            RelaySideConnectionId         = {};
    //
    size64_t                            TotalTcpBytesFromClient       = 0;
    size64_t                            TotalTcpBytesToClient         = 0;
    size64_t                            TotalUdpBytesFromClient       = 0;
    size64_t                            TotalUdpBytesToClient         = 0;
    //
    size_t                              TotalPostTcpBytesFromTarget   = 0;
    size_t                              TotalPostTcpBytesToTarget     = 0;
    size_t                              TotalConsumedTcpBytesByTarget = 0;
    bool                                IsSuspended                   = false;

    struct xHttpData {
        std::string TargetHost = {};
        uint16_t    TargetPort = {};
        std::string Content    = {};

        std::string TempHeaderLine    = {};
        size_t      ContentLengthLeft = 0;

        std::string ToString() const;
    } Http;

    struct {
        uint64_t StartupTimestampMS = 0;
    } Debug;

    using xTcpConnection::Clean;
    using xTcpConnection::GetLocalAddress;
    using xTcpConnection::GetRemoteAddress;
    using xTcpConnection::HasPendingWrites;
    using xTcpConnection::Init;
    using xTcpConnection::ResizeRecvBuffer;
    using xTcpConnection::ResizeSendBuffer;
    using xTcpConnection::ResumeReading;
    using xTcpConnection::SuspendReading;

    static xPA_ClientConnection * From(xTcpConnection * Connection) {
        return static_cast<xPA_ClientConnection *>(Connection);
    }
    void PostData(const void * DataPtr, size_t DataSize) {
        xTcpConnection::PostData(DataPtr, DataSize);
        TotalTcpBytesToClient += DataSize;
    }
    void PostUdpData(const void * DataPtr, size_t DataSize) {
        BindUdpChannel->PostData(BindUdpChannel->LastClientIpAddress, DataPtr, DataSize);
        TotalUdpBytesToClient += DataSize;
    }
};

class xProxyAccessService final
    : xTcpServer::iListener
    , xTcpConnection::iListener
    , xUdpChannel::iListener
    , public xProxyAbstractService {
public:
    struct xExportBindAddress {
        xNetAddress BindAddress;
        xNetAddress ExportIp;
    };

    bool Init(const std::string & AddressListFilename);
    bool Init(const std::vector<xExportBindAddress> & AddressList);
    void Clean();
    void Tick(uint64_t NowMS);
    void BindAuthService(xAuthAbstractService * Service) { AuthService = Service; }
    void BindDeviceLocatorService(xDeviceLocatorAbstractService * Service) { DeviceLocatorService = Service; }
    void BindRelayService(xRelayAbstractService * Service) { RelayService = Service; }
    void BindTargetReportService(xTargetReporterAbstractService * Service) { TargetReportService = Service; }
    void EnableUdp4(const xNetAddress & BindAddress, const xNetAddress & ExportAddress);
    void EnableUdp6(const xNetAddress & BindAddress, const xNetAddress & ExportAddress);
    void SetClientBufferSize(size_t Size);

    auto OutputAudit() -> std::string;

protected:  // override:
    void   OnNewConnection(xTcpServer * TcpServerPtr, xSocket && NativeHandle) override;
    void   OnConnected(xTcpConnection * TcpConnectionPtr) override;
    void   OnPeerClose(xTcpConnection * TcpConnectionPtr) override;
    void   OnFlush(xTcpConnection * TcpConnectionPtr) override;
    size_t OnData(xTcpConnection * TcpConnectionPtr, ubyte * DataPtr, size_t DataSize) override;
    void   OnData(xUdpChannel * UdpChannelPtr, ubyte * DataPtr, size_t DataSize, const xNetAddress & RemoteAddress) override;
    void   PostData(uint64_t ProxyClientConnectionId, const void * DataPtr, size_t DataSize) override;
    void   PostData(uint64_t ProxyClientUdpChannelId, const xNetAddress & SourceAddress, const void * DataPtr, size_t DataSize) override;
    void   CloseConnection(uint64_t ProxyClientConnectionId) override;
    void   UpdateConsumedTcpDataSizeByTarget(uint64_t ProxyClientConnectionId, size_t ConsumedSize) override;

protected:  // process data:
    void ProcessReadyAuthFuture();
    void ProcessReadyAcquireDeviceFuture();
    void ProcessReadyAcquireDeviceConnectionFuture();
    void ProcessReadyAcquireDeviceUdpChannelFuture();

    size_t KillConnectionOnData(xPA_ClientConnection * Connection, ubyte * DataPtr, size_t DataSize);
    size_t OnGuessProxyType(xPA_ClientConnection * Connection, ubyte * DataPtr, size_t DataSize);
    size_t OnS5Challenge(xPA_ClientConnection * Connection, ubyte * DataPtr, size_t DataSize);
    size_t OnS5AuthInfo(xPA_ClientConnection * Connection, ubyte * DataPtr, size_t DataSize);
    size_t OnS5Target(xPA_ClientConnection * Connection, ubyte * DataPtr, size_t DataSize);
    size_t OnRawData(xPA_ClientConnection * Connection, ubyte * DataPtr, size_t DataSize);
    size_t OnHttpChallenge(xPA_ClientConnection * Connection, ubyte * DataPtr, size_t DataSize);
    size_t OnHttpRawHeaderProcessor(xPA_ClientConnection * Connection, ubyte * DataPtr, size_t DataSize);
    size_t OnHttpNormalHeaderProcessor(xPA_ClientConnection * Connection, ubyte * DataPtr, size_t DataSize);
    size_t OnHttpBody(xPA_ClientConnection * Connection, ubyte * DataPtr, size_t DataSize);

    xPA_AuthFuture *                    RequestAuthentication(xPA_ClientConnection * Connection, std::string_view AuthView);
    xPA_AcquireDeviceFuture *           RequestDevice(xPA_ClientConnection * Connection, const xDeviceRequest & Request);
    xPA_AcquireDeviceConnectionFuture * RequestDeviceConnection(xPA_ClientConnection * Connection, const xNetAddress & TargetAddress);
    xPA_AcquireDeviceConnectionFuture * RequestDeviceConnection(xPA_ClientConnection * Connection, const std::string_view & TargetHostnameView, uint16_t TargetPort);
    xPA_AcquireDeviceUdpChannelFuture * RequestDeviceUdpChannel(xPA_ClientConnection * Connection);

    void PostRelayConnectionData(xPA_ClientConnection * LocalConnection, const void * DataPtr, size_t DataSize);

    void SendS5AuthError(xPA_ClientConnection * Connection);
    void SendS5AuthAccepted(xPA_ClientConnection * Connection);
    void SendS5AuthLimited(xPA_ClientConnection * Connection);
    void Send200(xPA_ClientConnection * Connection);
    void Send404(xPA_ClientConnection * Connection);
    void Send407(xPA_ClientConnection * Connection);
    void Send500(xPA_ClientConnection * Connection);
    void Send502(xPA_ClientConnection * Connection);
    void Send503(xPA_ClientConnection * Connection);

    void OnAuthResult(xPA_AuthFuture * Future);
    void OnS5AuthResult(xPA_ClientConnection * Connection, xPA_AuthFuture * Future);
    void OnHttpAuthResult(xPA_ClientConnection * Connection, xPA_AuthFuture * Future);
    void OnAcquireDeviceResult(xPA_AcquireDeviceFuture * Future);
    void OnS5AcquireDeviceResult(xPA_ClientConnection * Connection, xPA_AcquireDeviceFuture * Future);
    void OnHttpAcquireDeviceResult(xPA_ClientConnection * Connection, xPA_AcquireDeviceFuture * Future);
    void OnAcquireDeviceConnectionResult(xPA_AcquireDeviceConnectionFuture * Future);
    void OnS5AcquireDeviceConnectionResult(xPA_ClientConnection * Connection, xPA_AcquireDeviceConnectionFuture * Future);
    void OnHttpRawAcquireDeviceConnectionResult(xPA_ClientConnection * Connection, xPA_AcquireDeviceConnectionFuture * Future);
    void OnHttpNormalAcquireDeviceConnectionResult(xPA_ClientConnection * Connection, xPA_AcquireDeviceConnectionFuture * Future);
    void OnAcquireDeviceUdpChannelResult(xPA_AcquireDeviceUdpChannelFuture * Future);

protected:
    bool KeepAlive(xPA_ClientConnection * Connection);
    void DeferKill(xPA_ClientConnection * Connection);
    void DeferGracefulKill(xPA_ClientConnection * Connection);
    void ReleaseAuthFuture(xPA_ClientConnection * Connection);
    void ReleaseAcquireDeviceFuture(xPA_ClientConnection * Connection);
    void ReleaseAcquireDeviceConnectionFuture(xPA_ClientConnection * Connection);
    void ReleaseAcquireDeviceUdpChannelFuture(xPA_ClientConnection * Connection);
    void DestroyConnection(xPA_ClientConnection * Connection);

    void DeferKillInitTimeoutConnection();
    void DeferKillIdleTimeoutConnection();
    void DeferGracefulKillConnection();
    void ExcuteKillConnection();
    void ClearTimeoutFuture();

protected:
    void ProcessConsumedDataFeedback();

private:
    void ReportTarget(uint64_t GlobalAuthId, const xNetAddress & Address);
    void ReportTarget(uint64_t GlobalAuthId, const std::string_view & TargetHost);

private:
    struct xClientEntryServer : xTcpServer {
        xNetAddress ExportIp = {};
    };

    xTicker                           LocalTicker;
    size_t                            DefaultBufferSize = 0;
    std::vector<xClientEntryServer *> ClientEntryServerList;

    xIndexedStorage<xPA_ClientConnection>      ClientConnectionPool;
    xIndexedStorage<xPA_ClientUdpChannel>      ClientUdpChannelPool;
    xPA_ClientConnectionTimeoutList            ClientConnectionInitTimeoutList;
    xPA_ClientConnectionTimeoutList            ClientConnectionIdleTimeoutList;
    xPA_ClientConnectionTimeoutList            ClientConnectionKillList;
    xPA_ClientConnectionTimeoutList            ClientConnectionGracefulKillList;
    xPA_ClientConnectionUpdateConsumedDataList ClientConnectionConsumedDataSizeFeedbackList;

    xFuturePoolManager<xPA_AuthFuture>                    AuthFutureManager;
    xFuturePoolManager<xPA_AcquireDeviceFuture>           AcquireDeviceFutureManager;
    xFuturePoolManager<xPA_AcquireDeviceConnectionFuture> AcquireDeviceConnectionFutureManager;
    xFuturePoolManager<xPA_AcquireDeviceUdpChannelFuture> AcquireDeviceUdpChannelFutureManager;

    xAuthAbstractService *           AuthService          = nullptr;
    xDeviceLocatorAbstractService *  DeviceLocatorService = nullptr;
    xRelayAbstractService *          RelayService         = nullptr;
    xTargetReporterAbstractService * TargetReportService  = nullptr;

    xFutureList AuthFutureTimeoutList;
    xFutureList AcquireDeviceFutureTimeoutList;
    xFutureList AcquireDeviceConnectionFutureTimeoutList;
    xFutureList AcquireDeviceUdpChannelFutureTimeoutList;

    struct xAudit {
        size_t InvalidS5AuthTypeCount     = 0;
        size_t InvalidS5AuthInfo          = 0;
        size_t InvalidS5AuthResult        = 0;
        size_t InvalidS5Target            = 0;
        size_t LocalBindUdpChannelCount   = 0;
        size_t RequestAuthenticationOOM   = 0;
        size_t InitConnectionTimeoutCount = 0;

        uint64_t LastOutputTimestampMS = GetTimestampMS();
    } Audit;
};
