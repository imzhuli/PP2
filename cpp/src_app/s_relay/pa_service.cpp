#include "./pa_service.hpp"

#include <pp_common/service_runtime.hpp>

static constexpr const size_t MaxPAConnectionCount = 2'0000;

static xel::xTcpService PAService;

void InitPAService(const xNetAddress & BindAddress) {
    X_RUNTIME_ASSERT(PAService.Init(ServiceIoContext, BindAddress, MaxPAConnectionCount));
}

void CleanPAService() {
    PAService.Clean();
    return;
}

void PAServiceTicker(uint64_t NowMS) {
    TickAll(NowMS, PAService);
}
