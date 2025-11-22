#pragma once
#include "./_.hpp"
#include "./runtime_env.hpp"

extern xRuntimeEnv  RuntimeEnv;
extern xLogger *    Logger;
extern xLogger *    AuditLogger;
extern xIoContext * ServiceIoContext;
extern uint64_t     ServiceIoLoopOnceTimeoutMS;
extern xTicker      ServiceTicker;
extern xRunState    ServiceRunState;

struct xServiceRuntimeEnvGuard final : xNonCopyable {
    xServiceRuntimeEnvGuard(int argc, char ** argv, bool EnableDefaultLogger = true);
    ~xServiceRuntimeEnvGuard();

private:
    const bool EnableLogger;
};

template <typename... tTickerObserver>
void ServiceUpdateOnce(tTickerObserver &&... Observers) {
    ServiceTicker.Update();
    ServiceIoContext->LoopOnce(ServiceIoLoopOnceTimeoutMS);
    TickAll(ServiceTicker(), std::forward<tTickerObserver>(Observers)...);
}

/////////////////////////
class xServiceRequestContext;
class xServiceRequestContextPool;

class xServiceRequestContext : private xListNode {
    friend class xList<xServiceRequestContext>;

public:
    uint64_t          RequestId;
    uint64_t          RequestTimestampMS;
    mutable xVariable RequestContext;
    mutable xVariable RequestContextEx;
};
using xServiceRequestContextList = xList<xServiceRequestContext>;

class xServiceRequestContextPool {
public:
    bool Init(size_t PoolSize);
    void Clean();
    void Tick(uint64_t NowMS) { RemoveTimeoutRequests(DefaultTimeoutMS, NowMS); }

    auto Acquire(xVariable RequestContext = {}, xVariable RequestContextEx = {}) -> const xServiceRequestContext *;
    auto CheckAndGet(uint64_t RequestId) -> const xServiceRequestContext *;
    void Release(const xServiceRequestContext * RCP);
    void SetRequestTimeoutMS(uint64_t TimeoutMS) { DefaultTimeoutMS = TimeoutMS > 500 ? TimeoutMS : 500; }

    // callbacks
    using xOnTimeoutRequestCallback = std::function<void(const xServiceRequestContext *)>;
    //
    xOnTimeoutRequestCallback OnTimeoutRequestCallback = Noop<>;

    void RemoveTimeoutRequests(uint64_t TimeoutMS, uint64_t NowMS = ServiceTicker()) {
        auto KillTimepoint = NowMS - TimeoutMS;
        while (auto P = (const xServiceRequestContext *)TimeoutList.PopHead([KillTimepoint](const xServiceRequestContext & N) { return N.RequestTimestampMS <= KillTimepoint; })) {
            OnTimeoutRequestCallback(P);
            Pool.Release(P->RequestId);
        }
    }

private:
    xIndexedStorage<xServiceRequestContext> Pool;
    xServiceRequestContextList              TimeoutList;
    uint64_t                                DefaultTimeoutMS = 2'000;
};

class xTickRunner {
public:
    const uint64_t                          TimeoutMS;
    const std::function<void(uint64_t Now)> Function;

    xTickRunner(auto && F) : xTickRunner(0, std::forward<decltype(F)>(F)) {}
    xTickRunner(uint64_t TimeoutMS, auto && F) : TimeoutMS(TimeoutMS), Function(std::forward<decltype(F)>(F)) {}

    void Tick(uint64_t NowMS) {
        if (NowMS <= LastRunTimestampMS + TimeoutMS) {
            return;
        }
        LastRunTimestampMS = NowMS;
        Function(NowMS);
    }

private:
    uint64_t LastRunTimestampMS = 0;
};

#ifndef NDEBUG
#define DEBUG_LOG(fmt, ...) Logger->D("%s:%i:%s$ " fmt, std::filesystem::path(__FILE__).filename().c_str(), __LINE__, __func__, ##__VA_ARGS__)
#define DEBUG_ADT(fmt, ...) AuditLogger->D("%s:%i:%s$ " fmt, std::filesystem::path(__FILE__).filename().c_str(), __LINE__, __func__, ##__VA_ARGS__)
#else
#define DEBUG_LOG(...)
#define DEBUG_ADT(...)
#endif

#define SERVICE_RUNTIME_ASSERT(cond)                                                                 \
    do {                                                                                             \
        if (!cond) {                                                                                 \
            Logger->F("service assert: %i@%s, condition expression: %s", __LINE__, __FILE__, #cond); \
        }                                                                                            \
    } while (false)
