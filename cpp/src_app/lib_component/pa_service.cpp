#include "./pa_service.hpp"

#include <pp_common/service_runtime.hpp>

static constexpr const uint64_t PA_FUTURE_TIMEOUT_MS     = 1'000;
static constexpr const size_t   PA_MAX_CLIENT_CONNECTION = 10'0000;

bool xPA_Service::Init(const xNetAddress & BindAddress) {
    RuntimeAssert(ServiceRunState);
    if (ClientConnectionPool.Init(PA_MAX_CLIENT_CONNECTION)) {
        return false;
    }
    if (!TcpServer.Init(ServiceIoContext, BindAddress, this)) {
        ClientConnectionPool.Clean();
        return false;
    }
    return true;
}

void xPA_Service::Clean() {
    TcpServer.Clean();
    ClientConnectionPool.Clean();
}

void xPA_Service::Tick(uint64_t NowMS) {
    LocalTicker.Update(NowMS);
}

void xPA_Service::ClearTimeoutFuture() {
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
void xPA_Service::OnNewConnection(xTcpServer * TcpServerPtr, xSocket && NativeHandle) {
}

// void xPA_Service::OnConnected(xTcpConnection * TcpConnectionPtr) override;
// void xPA_Service::OnPeerClose(xTcpConnection * TcpConnectionPtr) override;
