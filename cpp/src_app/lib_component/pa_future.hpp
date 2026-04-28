#pragma once
#include "./auth_abstract.hpp"
#include "./pa_abstract.hpp"

#include <expected>
#include <pp_common/future.hpp>

struct xPA_FutureBase : xFutureBase {
    enum eType {
        None = 0,
        Auth,
        AcquireDevice,
        AcquireDeviceConnection,
        AcquireDeviceUdpChannel
    };
    eType                  Type             = eType::None;
    xPA_ClientConnection * ClientConnection = nullptr;

protected:
    xPA_FutureBase() = default;
};

struct xPA_AuthFuture final : xPA_FutureBase {
    enum struct eErrorCode : uint16_t {
        OK      = 0,
        NO_DATA = 1,
        OOM     = 2,
    };
    using xResult = std::expected<xAuthResult *, eErrorCode>;

    xResult Result = std::unexpected(eErrorCode::NO_DATA);

    xPA_AuthFuture() { Type = eType::Auth; }
};

struct xPA_AcquireDeviceFuture final : xPA_FutureBase {
    xPA_AcquireDeviceFuture() { Type = eType::AcquireDevice; }
};

struct xPA_AcquireDeviceConnectionFuture final : xPA_FutureBase {
    uint64_t RelaySideConnectionId = 0;

    xPA_AcquireDeviceConnectionFuture() { Type = eType::AcquireDeviceConnection; }
};

struct xPA_AcquireDeviceUdpChannelFuture final : xPA_FutureBase {
    uint64_t RelaySideChannelId = 0;

    xPA_AcquireDeviceUdpChannelFuture() { Type = eType::AcquireDeviceUdpChannel; }
};
