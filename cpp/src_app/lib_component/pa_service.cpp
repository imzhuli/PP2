#include "./pa_service.hpp"

#include <pp_common/service_runtime.hpp>

static constexpr const uint64_t PA_FUTURE_TIMEOUT_MS             = 1'000;
static constexpr const size_t   PA_MAX_CLIENT_CONNECTION         = 20'0000;
static constexpr const size_t   PA_MAX_CLIENT_REQUEST_PER_SECOND = 5'0000;

bool xProxyAccessService::Init(const xNetAddress & BindAddress4, const xNetAddress & BindAddress6) {
    X_RUNTIME_ASSERT(ServiceRunState);
    if (!ClientConnectionPool.Init(PA_MAX_CLIENT_CONNECTION)) {
        DEBUG_LOG("ClientConnectionPool init error");
        return false;
    }
    auto ClientConnectionPoolCleaner = xScopeCleaner(ClientConnectionPool);

    if (!ClientUdpChannelPool.Init(PA_MAX_CLIENT_CONNECTION)) {
        DEBUG_LOG("ClientUdpChannelPool init error");
        return false;
    }
    auto ClientUdpChannelPoolCleaner = xScopeCleaner(ClientUdpChannelPool);

    X_RUNTIME_ASSERT(BindAddress4 || BindAddress6);
    auto TcpServerCleaner = xScopeGuard([this] {
        if (TcpServer4) {
            TcpServer4->Clean();
            delete Steal(TcpServer4);
        }
        if (TcpServer6) {
            TcpServer6->Clean();
            delete Steal(TcpServer6);
        }
    });
    if (BindAddress4) {
        TcpServer4 = new xTcpServer();
        if (!TcpServer4->Init(ServiceIoContext, BindAddress4, this)) {
            DEBUG_LOG("TcpServer4 init error");
            delete Steal(TcpServer4);
            return false;
        }
    }
    if (BindAddress6) {
        TcpServer6 = new xTcpServer();
        if (!TcpServer6->Init(ServiceIoContext, BindAddress6, this)) {
            DEBUG_LOG("TcpServer4 init error");
            delete Steal(TcpServer6);
            return false;
        }
    }

    if (!AuthFutureManager.Init(PA_MAX_CLIENT_REQUEST_PER_SECOND)) {
        DEBUG_LOG("AuthFutureManager init error");
        return false;
    }
    auto AuthFutureManagerCleaner = xScopeCleaner(AuthFutureManager);

    if (!AcquireDeviceFutureManager.Init(PA_MAX_CLIENT_REQUEST_PER_SECOND)) {
        DEBUG_LOG("AcquireDeviceFutureManager init error");
        return false;
    }
    auto AcquireDeviceFutureManagerCleaner = xScopeCleaner(AcquireDeviceFutureManager);

    if (!CreateDeviceConnectionFutureManager.Init(PA_MAX_CLIENT_REQUEST_PER_SECOND)) {
        DEBUG_LOG("CreateDeviceConnectionFutureManager init error");
        return false;
    }
    auto CreateDeviceConnectionFutureManagerCleaner = xScopeCleaner(CreateDeviceConnectionFutureManager);

    if (!CreateDeviceUdpChannelFutureManager.Init(PA_MAX_CLIENT_REQUEST_PER_SECOND)) {
        DEBUG_LOG("CreateDeviceUdpChannelFutureManager init error");
        return false;
    }
    auto CreateDeviceUdpChannelFutureManagerCleaner = xScopeCleaner(CreateDeviceUdpChannelFutureManager);

    ClientConnectionPoolCleaner.Dismiss();
    TcpServerCleaner.Dismiss();
    AuthFutureManagerCleaner.Dismiss();
    CreateDeviceConnectionFutureManagerCleaner.Dismiss();
    CreateDeviceUdpChannelFutureManagerCleaner.Dismiss();

    Reset(BindUdpAddress4);
    Reset(BindUdpAddress6);
    Reset(ExportUdpAddress4);
    Reset(ExportUdpAddress6);

    return true;
}

void xProxyAccessService::Clean() {
    AuthFutureManager.Clean();
    CreateDeviceConnectionFutureManager.Clean();
    CreateDeviceUdpChannelFutureManager.Clean();
    do {  // clean tcp servers
        if (TcpServer4) {
            TcpServer4->Clean();
            delete Steal(TcpServer4);
        }
        if (TcpServer6) {
            TcpServer6->Clean();
            delete Steal(TcpServer6);
        }
    } while (false);
    Reset(BindUdpAddress4);
    Reset(BindUdpAddress6);
    Reset(ExportUdpAddress4);
    Reset(ExportUdpAddress6);
    ClientUdpChannelPool.Clean();
    ClientConnectionPool.Clean();
}

void xProxyAccessService::Tick(uint64_t NowMS) {
    LocalTicker.Update(NowMS);
}

void xProxyAccessService::EnableUdp4(const xNetAddress & BindAddress, const xNetAddress & ExportAddress) {
    X_RUNTIME_ASSERT(BindAddress.Is4() && !BindAddress.Port && ExportAddress.Is4() && !ExportAddress.Port);
    BindUdpAddress4   = BindAddress;
    ExportUdpAddress4 = ExportAddress;
    Logger->I("EnableUdp4: %s -> %s", BindAddress.IpToString().c_str(), ExportAddress.IpToString().c_str());
}

void xProxyAccessService::EnableUdp6(const xNetAddress & BindAddress, const xNetAddress & ExportAddress) {
    X_RUNTIME_ASSERT(BindAddress.Is6() && !BindAddress.Port && ExportAddress.Is6() && !ExportAddress.Port);
    BindUdpAddress6   = BindAddress;
    ExportUdpAddress6 = ExportAddress;
    Logger->I("EnableUdp6: %s -> %s", BindAddress.IpToString().c_str(), ExportAddress.IpToString().c_str());
}

void xProxyAccessService::KeepAlive(xPA_ClientConnection * Connection) {
    if (Connection->DeleteMark) {
        return;
    }
    Connection->TimestampMS = LocalTicker();
    ClientConnectionTimeoutList.GrabTail(*Connection);
}

void xProxyAccessService::DeferKill(xPA_ClientConnection * Connection) {
    if (Steal(Connection->DeleteMark, true)) {
        return;
    }
    ClientConnectionKillList.GrabTail(*Connection);
}

void xProxyAccessService::DestroyConnection(xPA_ClientConnection * Connection) {
    auto ConnectionId = Connection->ConnectionId;
    Connection->Clean();
    ClientConnectionPool.Release(ConnectionId);
}

void xProxyAccessService::ClearTimeoutFuture() {
    auto KillTimepoint = LocalTicker() - PA_FUTURE_TIMEOUT_MS;
    auto Cond          = [this, KillTimepoint](const xFutureNode & F) {
        return F.StartTimestampMS <= KillTimepoint;
    };
    while (auto P = static_cast<xPA_AuthFuture *>(AuthFutureTimeoutList.PopHead(Cond))) {
        Pass(P);
    }
    while (auto P = static_cast<xPA_CreateDeviceConnectionFuture *>(CreateDeviceConnectionFutureTimeoutList.PopHead(Cond))) {
        Pass(P);
    }
    while (auto P = static_cast<xPA_CreateDeviceUdpChannelFuture *>(CreateDeviceUdpChannelFutureTimeoutList.PopHead(Cond))) {
        Pass(P);
    }
}

// tcp server listener:
void xProxyAccessService::OnNewConnection(xTcpServer * TcpServerPtr, xSocket && NativeHandle) {
    auto ConnectionId = ClientConnectionPool.Acquire();
    if (!ConnectionId) {
        XelCloseSocket(NativeHandle);
        return;
    }
    auto & ClientConnection = ClientConnectionPool[ConnectionId];
    if (!ClientConnection.Init(ServiceIoContext, std::move(NativeHandle), this)) {
        ClientConnectionPool.Release(ConnectionId);
        return;
    }
    ClientConnection.DataProcessor = &xProxyAccessService::OnGuessProxyType;
}

void xProxyAccessService::OnConnected(xTcpConnection * TcpConnectionPtr) {
    Unreachable();
}

void xProxyAccessService::OnPeerClose(xTcpConnection * TcpConnectionPtr) {
    Todo(__FILE__);
}

void xProxyAccessService::OnFlush(xTcpConnection * TcpConnectionPtr) {
    Pass();  // speed control
}

size_t xProxyAccessService::OnData(xTcpConnection * TcpConnectionPtr, ubyte * DataPtr, size_t DataSize) {
    auto ClientConnection = static_cast<xPA_ClientConnection *>(TcpConnectionPtr);
    return (this->*ClientConnection->DataProcessor)(ClientConnection, DataPtr, DataSize);
}

void xProxyAccessService::OnData(xUdpChannel * UdpChannelPtr, ubyte * DataPtr, size_t DataSize, const xNetAddress & RemoteAddress) {
    Pass();
}

size_t xProxyAccessService::OnGuessProxyType(xPA_ClientConnection * Connection, ubyte * DataPtr, size_t DataSize) {
    if (DataSize < 3) {
        DEBUG_LOG("invalid challenge size");
        return InvalidDataSize;
    }
    return InvalidDataSize;
}
