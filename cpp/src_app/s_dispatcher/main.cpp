#include "../lib_component/request_mapping.hpp"

#include <map>
#include <pp_common/service_runtime.hpp>

struct xReceiverNode : xListNode {
    xTcpServiceClientConnectionHandle ClientHandle;
};
using xReceiverList = xList<xReceiverNode>;

struct xDispatcher {
    xReceiverList ConsumerHandleList;
    xReceiverNode ObserverHandleList;
};
std::map<xPacketCommandId, xDispatcher> CommandDispatcherMap;

struct xSourceRequest : xListNode {
    uint64_t                          RequestId;
    xTcpServiceClientConnectionHandle SourceHandle;
    uint64_t                          SourceRequestId;
    uint64_t                          SourceRequestTimestampMS;
};
static xTcpServiceRequestMap SourceRequestMap;
static X_RESOURCE_GUARD_ASSERTED(SourceRequestMap, 10'0000);

int main(int argc, char ** argv) {
#ifndef NDEBUG
    X_VAR xServiceEnvironmentGuard(argc, argv, ServiceConsoleLogger);
#else
    X_VAR xServiceEnvironmentGuard(argc, argv);
#endif
    auto CL = ServiceLoadConfig();

    SourceRequestMap.Acquire({}, {});
    SourceRequestMap.Acquire({}, {});
    auto R1 = SourceRequestMap.Acquire({}, {});
    auto R2 = SourceRequestMap.Acquire({}, {});

    auto T     = xTimer();
    auto Ticks = size_t(0);
    auto R     = false;
    while (ServiceRunState) {
        ServiceUpdateOnce(SourceRequestMap);
        if (T.TestAndTag(500ms)) {
            ++Ticks;
            if (!R && Ticks > 1) {
                SourceRequestMap.GetAndRelease(R1);
                SourceRequestMap.GetAndRelease(R2);
                R = true;
            }
            cout << SourceRequestMap.GetAudit().TotalRequestCount << endl;
        }
    }
}
