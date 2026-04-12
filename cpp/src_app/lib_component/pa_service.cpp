#include "./pa_service.hpp"

#include <pp_common/service_runtime.hpp>

static constexpr const uint64_t PA_FUTURE_TIMEOUT_MS = 1'000;

void xPA_ServiceBase::ClearTimeoutFuture() {
    auto KillTimepoint = LocalTicker() - PA_FUTURE_TIMEOUT_MS;
    auto Cond          = [this, KillTimepoint](const xFutureNode & F) {
        return F.StartTimestampMS <= KillTimepoint;
    };
    while (auto P = static_cast<xPA_AuthFeature *>(AuthFutureTimeoutList.PopHead(Cond))) {
        Pass(P);
    }
    while (auto P = static_cast<xPA_CreateRelayTcpConnectionFeature *>(CreateRelayTcpConnectionFeatureTimeoutList.PopHead(Cond))) {
        Pass(P);
    }
    while (auto P = static_cast<xPA_CreateRelayUdpChannelFeature *>(CreateRelayUdpChannelFeatureTimeoutList.PopHead(Cond))) {
        Pass(P);
    }
}
