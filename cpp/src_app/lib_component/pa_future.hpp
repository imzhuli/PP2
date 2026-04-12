#pragma once
#include <pp_common/future.hpp>

struct xPA_AuthFeature final : xFutureBase {
    uint64_t AuthRequestId = 0;
};

struct xPA_CreateRelayTcpConnectionFeature final : xFutureBase {
    uint64_t RelaySideConnectionId = 0;
};

struct xPA_CreateRelayUdpChannelFeature final : xFutureBase {
    uint64_t RelaySideChannelId = 0;
};
