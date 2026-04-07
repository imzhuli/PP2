#include "../lib_component/request_mapping.hpp"

#include <bitset>
#include <map>
#include <pp_common/service_runtime.hpp>
#include <pp_protocol/command.hpp>

namespace audit {
    size_t NoConsumerFound        = 0;
    size_t RequestMappingOverflow = 0;

    uint64_t LastAuditTimestampMS = 0;
}  // namespace audit

static constexpr const size_t MAX_COMMAND_ID_COUNT = 128;
using xCommandBitset                               = std::bitset<MAX_COMMAND_ID_COUNT>;

struct xReceiverNode : xListNode {
    xCommandBitset                    CommandIdMask = xCommandBitset().flip();
    xTcpServiceClientConnectionHandle ClientHandle;
};
using xReceiverList = xList<xReceiverNode>;

struct xDispatcher {
    xCommandBitset CommandIdMask;
    bool           RequireResponse = false;
};
static std::map<xPacketCommandId, xDispatcher> CommandDispatcherMap;
static xReceiverList                           ConsumerHandleList;
static xReceiverList                           ObserverHandleList;

struct xSourceRequest : xListNode {
    uint64_t                          RequestId;
    xTcpServiceClientConnectionHandle SourceHandle;
    uint64_t                          SourceRequestId;
    uint64_t                          SourceRequestTimestampMS;
};

static xTcpServiceRequestMap SourceRequestMap;
static X_RESOURCE_GUARD_ASSERTED(SourceRequestMap, 10'0000);

static xNetAddress ProducerAddress;
static xNetAddress ConsumerAddress;
static xNetAddress ObserverAddress;

static xTcpService ProducerService;
static xTcpService ConsumerService;
static xTcpService ObserverService;

static auto NextEnableCommandIdBitmask = xCommandBitset(0x01);
static void EnableCommand(xPacketCommandId CmdId, bool RequireResponse) {
    auto & N                     = CommandDispatcherMap[CmdId];
    N.CommandIdMask              = NextEnableCommandIdBitmask;
    N.RequireResponse            = RequireResponse;
    NextEnableCommandIdBitmask <<= 1;
    X_RUNTIME_ASSERT(NextEnableCommandIdBitmask.any());
}

static void InitDispatcher() {
    EnableCommand(Cmd_EchoTest, true);
}

static void CleanDispatcher() {
    Reset(CommandDispatcherMap);
}

static void DispatchPacket(const xTcpServiceClientConnectionHandle & SourceHandle, xPacketCommandId CmdId, xPacketRequestId RequestId, ubyte * Payload, size_t PayloadSize) {
    SERVICE_RUNTIME_ASSERT(PayloadSize <= MaxPacketPayloadSize);

    auto Iter = CommandDispatcherMap.find(CmdId);
    if (Iter == CommandDispatcherMap.end()) {
        ++audit::RequestMappingOverflow;
        return;
    }
    auto & Dispatcher = Iter->second;
    // build and dispatch to observers first:
    ubyte DataBuffer[MaxPacketSize];
    auto  PostSize = BuildPacket(DataBuffer, CmdId, RequestId, Payload, PayloadSize);
    ObserverHandleList.ForEach([&](xReceiverNode & Node) {
        if ((Node.CommandIdMask & Dispatcher.CommandIdMask).any()) {
            Node.ClientHandle.PostData(DataBuffer, PostSize);
        }
    });

    auto Tested   = xReceiverList();
    auto Consumer = static_cast<xReceiverNode *>(nullptr);
    while (auto HeadConsumer = ConsumerHandleList.PopHead()) {
        Tested.AddTail(*HeadConsumer);
        if ((HeadConsumer->CommandIdMask & Dispatcher.CommandIdMask).none()) {
            continue;
        }
        Consumer = HeadConsumer;
        break;
    }
    ConsumerHandleList.GrabListTail(Tested);
    if (!Consumer) {
        if (Dispatcher.RequireResponse) {
            ++audit::NoConsumerFound;
        }
        return;
    }
    if (Dispatcher.RequireResponse) {
        auto ReplacedRequestId = SourceRequestMap.Acquire(SourceHandle, RequestId);
        if (!ReplacedRequestId) {
            ++audit::NoConsumerFound;
            return;
        }
        xPacketHeader::PatchRequestId(DataBuffer, ReplacedRequestId);
    }
    Consumer->ClientHandle.PostData(DataBuffer, PostSize);
}

static bool OnProducerPacket(const xTcpServiceClientConnectionHandle & SourceHandle, xPacketCommandId CmdId, xPacketRequestId RequestId, ubyte * Payload, size_t PayloadSize) {
    DispatchPacket(SourceHandle, CmdId, RequestId, Payload, PayloadSize);
    return true;
}

static void OnConsumerConnected(const xTcpServiceClientConnectionHandle & SourceHandle) {
    auto RN = new (std::nothrow) xReceiverNode();
    if (!RN) {
        SourceHandle.Kill();
        return;
    }
    RN->ClientHandle = SourceHandle;
    ConsumerHandleList.AddTail(*RN);
    SourceHandle->UserContext.P = RN;
}

static bool OnConsumerPacket(const xTcpServiceClientConnectionHandle & SourceHandle, xPacketCommandId CmdId, xPacketRequestId RequestId, ubyte * Payload, size_t PayloadSize) {
    auto Source = SourceRequestMap.GetAndRelease(RequestId);
    if (!Source.SourceHandle.IsValid()) {
        DEBUG_LOG("no source found, response requestId=%" PRIx64 "", RequestId);
        return true;
    }
    ubyte DataBuffer[MaxPacketSize];
    auto  PostSize = BuildPacket(DataBuffer, CmdId, Source.SourceRequestId, Payload, PayloadSize);
    Source.SourceHandle.PostData(DataBuffer, PostSize);
    DEBUG_LOG("ResponseRequestId=%" PRIx64 " -> SourceRequestId=%" PRIx64 "", RequestId, Source.SourceRequestId);
    return true;
}

static void OnConsumerClean(const xTcpServiceClientConnectionHandle & SourceHandle) {
    DEBUG_LOG();
    auto RN = static_cast<xReceiverNode *>(SourceHandle->UserContext.P);
    delete RN;
}

static void OnObserverConnected(const xTcpServiceClientConnectionHandle & SourceHandle) {
    auto RN = new (std::nothrow) xReceiverNode();
    if (!RN) {
        SourceHandle.Kill();
        return;
    }
    RN->ClientHandle = SourceHandle;
    ObserverHandleList.AddTail(*RN);
    SourceHandle->UserContext.P = RN;
}

static bool OnObserverPacket(const xTcpServiceClientConnectionHandle & SourceHandle, xPacketCommandId CmdId, xPacketRequestId RequestId, ubyte * Payload, size_t PayloadSize) {
    return true;
}

static void OnObserverClean(const xTcpServiceClientConnectionHandle & SourceHandle) {
    DEBUG_LOG();
    auto RN = static_cast<xReceiverNode *>(SourceHandle->UserContext.P);
    delete RN;
}

static void AuditTick(uint64_t NowMS) {
    if (audit::LastAuditTimestampMS < NowMS - 60'000) {
        return;
    }
    audit::LastAuditTimestampMS = NowMS;

    auto DoLog = false;
    DoLog     |= audit::NoConsumerFound;
    DoLog     |= audit::RequestMappingOverflow;
    if (!DoLog) {
        return;
    }
    AuditLogger->I("NoConsumerFound=%zi, RequestMappingOverflow=%zi", Steal(audit::NoConsumerFound), Steal(audit::RequestMappingOverflow));
}

int main(int argc, char ** argv) {
    X_VAR xServiceEnvironmentGuard(argc, argv);

    auto CL = ServiceLoadConfig();
    CL.Require(ProducerAddress, "ProducerAddress");
    CL.Require(ConsumerAddress, "ConsumerAddress");
    CL.Require(ObserverAddress, "ObserverAddress");

    X_RESOURCE_GUARD_ASSERTED(ProducerService, ServiceIoContext, ProducerAddress, 5000);
    X_RESOURCE_GUARD_ASSERTED(ConsumerService, ServiceIoContext, ConsumerAddress, 100);
    X_RESOURCE_GUARD_ASSERTED(ObserverService, ServiceIoContext, ObserverAddress, 100);

    ProducerService.OnClientPacket = OnProducerPacket;

    ConsumerService.OnClientConnected = OnConsumerConnected;
    ConsumerService.OnClientPacket    = OnConsumerPacket;
    ConsumerService.OnClientClean     = OnConsumerClean;

    ConsumerService.OnClientConnected = OnObserverConnected;
    ObserverService.OnClientPacket    = OnObserverPacket;
    ConsumerService.OnClientClean     = OnObserverClean;

    X_SCOPE_GUARD(InitDispatcher, CleanDispatcher);

    while (ServiceRunState) {
        ServiceUpdateOnce(SourceRequestMap, ProducerService, ConsumerService, ObserverService, AuditTick);
    }
}
