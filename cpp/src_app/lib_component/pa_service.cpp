#include "./pa_service.hpp"

#include <pp_common/service_runtime.hpp>

static constexpr const uint64_t PA_FUTURE_TIMEOUT_MS             = 1'000;
static constexpr const size_t   PA_MAX_CLIENT_CONNECTION         = 20'0000;
static constexpr const size_t   PA_MAX_CLIENT_REQUEST_PER_SECOND = 5'0000;

bool xProxyAccessService::Init(const xNetAddress & BindAddress) {
    RuntimeAssert(ServiceRunState);
    if (ClientConnectionPool.Init(PA_MAX_CLIENT_CONNECTION)) {
        return false;
    }
    auto ClientConnectionPoolCleaner = xScopeCleaner(AuthFutureManager);

    if (!TcpServer.Init(ServiceIoContext, BindAddress, this)) {
        return false;
    }
    auto TcpServerCleaner = xScopeCleaner(TcpServer);

    if (!AuthFutureManager.Init(PA_MAX_CLIENT_REQUEST_PER_SECOND)) {
        return false;
    }
    auto AuthFutureManagerCleaner = xScopeCleaner(AuthFutureManager);

    if (!AcquireDeviceFutureManager.Init(PA_MAX_CLIENT_REQUEST_PER_SECOND)) {
        return false;
    }
    auto AcquireDeviceFutureManagerCleaner = xScopeCleaner(AcquireDeviceFutureManager);

    if (!CreateDeviceConnectionFutureManager.Init(PA_MAX_CLIENT_REQUEST_PER_SECOND)) {
        return false;
    }
    auto CreateDeviceConnectionFutureManagerCleaner = xScopeCleaner(CreateDeviceConnectionFutureManager);

    if (!CreateDeviceUdpChannelFutureManager.Init(PA_MAX_CLIENT_REQUEST_PER_SECOND)) {
        return false;
    }
    auto CreateDeviceUdpChannelFutureManagerCleaner = xScopeCleaner(CreateDeviceUdpChannelFutureManager);

    ClientConnectionPoolCleaner.Dismiss();
    TcpServerCleaner.Dismiss();
    AuthFutureManagerCleaner.Dismiss();
    CreateDeviceConnectionFutureManagerCleaner.Dismiss();
    CreateDeviceUdpChannelFutureManagerCleaner.Dismiss();

    return true;
}

void xProxyAccessService::Clean() {
    AuthFutureManager.Clean();
    CreateDeviceConnectionFutureManager.Clean();
    CreateDeviceUdpChannelFutureManager.Clean();
    TcpServer.Clean();
    ClientConnectionPool.Clean();
}

void xProxyAccessService::Tick(uint64_t NowMS) {
    LocalTicker.Update(NowMS);
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
    return DataSize;
}

void xProxyAccessService::OnData(xUdpChannel * ChannelPtr, ubyte * DataPtr, size_t DataSize, const xNetAddress & RemoteAddress) {
    Pass();
}
