#include "./pa_service.hpp"

#include <pp_common/service_runtime.hpp>

static constexpr const size_t MaxPAConnectionCount = 2'0000;

static xel::xTcpService PAService;

void InitPAService(const xNetAddress & BindAddress) {
    RuntimeAssert(PAService.Init(ServiceIoContext, BindAddress, MaxPAConnectionCount));
}

void CleanPAService() {
    PAService.Clean();
    return;
}

void PAServiceTicker(uint64_t NowMS) {
}
