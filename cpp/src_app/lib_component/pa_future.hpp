#pragma once
#include "./auth_abstract.hpp"
#include "./pa_abstract.hpp"

#include <expected>
#include <pp_common/future.hpp>

struct xPA_FutureBase : xFutureBase {
    xPA_ClientConnection * ClientConnection = nullptr;
};

struct xPA_AuthFuture final : xFutureBase {
    enum struct eErrorCode : uint16_t {
        OK      = 0,
        NO_DATA = 1,
        OOM     = 2,
    };
    using xResult = std::expected<xel::xIndexId /* ResultId */, eErrorCode>;

    uint64_t AuthRequestId = 0;
    xResult  Result        = std::unexpected(eErrorCode::NO_DATA);
};

struct xPA_AcquireDeviceFuture final : xFutureBase {
};

struct xPA_CreateDeviceConnectionFuture final : xFutureBase {
    uint64_t RelaySideConnectionId = 0;
};

struct xPA_CreateDeviceUdpChannelFuture final : xFutureBase {
    uint64_t RelaySideChannelId = 0;
};
