#include "./observer_port.hpp"

#include "./config.hpp"

static constexpr const uint64_t NEW_OBSERVER_PHASE_TIMEOUT = 225'000;  // 2.5 * xTcpClient_MaxKeepAliveTimeoutMS

static xRID_ObserverContext ObserverContextMap[MAX_OBSERVER_CONTEXT_COUNT];
static xTcpService          ObserverService;

void InitObserverPort() {
    RuntimeAssert(ObserverService.Init(ServiceIoContext, ObserverPortBindAddress4, MAX_OBSERVER_CONTEXT_COUNT));
}

void CleanObserverPort() {
    ObserverService.Clean();
}

void ObserverPortTick(uint64_t NowMS) {
    ObserverService.Tick(NowMS);
}