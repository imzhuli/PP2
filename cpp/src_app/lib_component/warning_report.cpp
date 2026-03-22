#include "./warning_report.hpp"

#include <pp_common/service_runtime.hpp>

bool xRemoteWarning::Init(const xNetAddress & Remote) {
    X_RUNTIME_ASSERT(ServiceRunState);
    if (!Client.Init(ServiceIoContext, Remote)) {
        return false;
    }
    for (auto & W : Warnings) {
        Reset(W);
    }
    return true;
}

void xRemoteWarning::Clean() {
    Client.Clean();
}

void xRemoteWarning::Tick(uint64_t NowMS) {
    Client.Tick(NowMS);
}

void xRemoteWarning::Warn(size_t Index) {
    X_RUNTIME_ASSERT(Index <= MAX_WARNING);
    auto & W   = Warnings[Index];
    auto   Now = ServiceTicker();
    if (Now - W.LastReportTimestamp > W.TimeoutLimitMS) {  // reset timtout accumulates
        W.LastReportTimestamp = Now;
        W.Count               = 0;
    }
    if (++W.Count == W.Threshold) {
        DoWarn(W);
    }
}

void xRemoteWarning::SetupWarning(size_t Index, size_t Threshold, uint64_t TimeoutMS) {
    X_RUNTIME_ASSERT(Index <= MAX_WARNING);
    X_RUNTIME_ASSERT(Threshold);
    X_RUNTIME_ASSERT(TimeoutMS);
    auto & W              = Warnings[Index];
    W.TimeoutLimitMS      = TimeoutMS;
    W.Threshold           = Threshold;
    W.Count               = 0;
    W.LastReportTimestamp = ServiceTicker();
}

void xRemoteWarning::DoWarn(const xWarning & W) {
    if (!Client.IsOpen()) {
        return;
    }
}
