#pragma once
#include "./abstract/auth_abstract.hpp"
#include "./abstract/device_abstract.hpp"
#include "./abstract/pa_abstract.hpp"

#include <expected>
#include <pp_common/future.hpp>

struct xPA_AuthFuture final : xAuthResultFuture {
    struct xPA_ClientConnection * ClientConnection = nullptr;
};

struct xPA_AcquireDeviceFuture final : xAcquireDeviceFuture {
    struct xPA_ClientConnection * ClientConnection  = nullptr;
    uint64_t                      RelayServerid     = 0;
    uint64_t                      RelaySideDeviceId = 0;
};

struct xPA_AcquireDeviceConnectionFuture final : xFutureBase {
    uint64_t RelaySideConnectionId = 0;
};

struct xPA_AcquireDeviceUdpChannelFuture final : xFutureBase {
    uint64_t RelaySideChannelId = 0;
};
