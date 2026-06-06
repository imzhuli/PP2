#include "./reporter.hpp"

#include "../lib_util/rdkafka_wrapper.hpp"

#include <pp_common/service_runtime.hpp>
#include <pp_protocol/_backend/kfk/audit_collect.hpp>
#include <pp_protocol/command.hpp>

static constexpr size_t MAX_AUDIT_REPORTER_COUNT = 20'0000;

struct xKfkContext {
    xKfkProducer KR                  = {};
    std::string  SecurityProtocol    = {};
    std::string  SaslMechanism       = {};
    std::string  SaslUsername        = {};
    std::string  SaslPassword        = {};
    std::string  BootstrapServerList = {};
    std::string  Topic               = {};
};

std::string xAuditCollectReporter::xAuditCollectNode::ToString() const {
    auto OS = std::ostringstream();
    OS << "xAuditCollectNode:" << endl;
    OS << "\t" << UsageInfo.ToString() << endl;
    return OS.str();
}

bool xAuditCollectReporter::Init(const std::string & ConfigFilename) {

    if (!NodePool.Init({ .InitSize = MAX_AUDIT_REPORTER_COUNT, .MaxPoolSize = MAX_AUDIT_REPORTER_COUNT })) {
        return false;
    }

    KfkContext = new xKfkContext();
    auto CL    = xel::xConfigLoader(ConfigFilename);

    CL.Require(KfkContext->SecurityProtocol, "SecurityProtocol");
    CL.Require(KfkContext->SaslMechanism, "SaslMechanism");
    CL.Require(KfkContext->SaslUsername, "SaslUsername");
    CL.Require(KfkContext->SaslPassword, "SaslPassword");
    CL.Require(KfkContext->BootstrapServerList, "BootstrapServerList");
    CL.Require(KfkContext->Topic, "Topic");

    if (!KfkContext->KR.Init(
            KfkContext->Topic,
            {
                { "security.protocol", KfkContext->SecurityProtocol },
                { "sasl.mechanism", KfkContext->SaslMechanism },
                { "sasl.username", KfkContext->SaslUsername },
                { "sasl.password", KfkContext->SaslPassword },
                { "bootstrap.servers", KfkContext->BootstrapServerList },
            }
        )) {
        delete Steal(KfkContext);
        NodePool.Clean();
        return false;
    }

    RunState.Start();
    KfkThread = std::thread([this] {
        KfkThreadFunc();
    });
    return true;
}

void xAuditCollectReporter::Clean() {
    RunState.Stop();
    Steal(KfkThread).join();
    do {  // clear resources
        xAuditCollectList FreeCollectionList;
        FreeCollectionList.GrabListTail(PostCollectionList);
        FreeCollectionList.GrabListTail(PendingCollectionList);
        FreeCollectionList.GrabListTail(FinishedCollectionList);
        while (auto Node = FreeCollectionList.PopHead()) {
            NodePool.Destroy(Node);
        }
    } while (false);
    KfkContext->KR.Clean();
    delete Steal(KfkContext);
    NodePool.Clean();
}

void xAuditCollectReporter::Tick(uint64_t NowMS) {
    xAuditCollectList FreeCollectionList;
    do {
        X_VAR xSpinlockGuard(SwitchListLock);
        PendingCollectionList.GrabListTail(PostCollectionList);
        FreeCollectionList.GrabListTail(FinishedCollectionList);
    } while (false);
    while (auto Node = FreeCollectionList.PopHead()) {
        NodePool.Destroy(Node);
    }
}

void xAuditCollectReporter::PostAuditCollect(const xAuditUsage & UsageInfo) {
    auto Node = NodePool.Create();
    if (!Node) {
        DEBUG_LOG("%s", "NodePool OOM");
        return;
    }
    Node->UsageInfo = UsageInfo;
    PostCollectionList.AddTail(*Node);
}

void xAuditCollectReporter::KfkThreadFunc() {
    xAuditCollectList TempPostList;
    xAuditCollectList TempFinishedList;

    ubyte Buffer[MaxPacketSize];
    while (RunState) {
        assert(TempFinishedList.IsEmpty());
        while (auto PNode = TempPostList.PopHead()) {
            // do post:
            DEBUG_LOG("%s", PNode->ToString().c_str());

            auto R            = xPPB_AuditUsage();
            R.TimeMs          = PNode->UsageInfo.StartTimestampMS;
            R.AuditId         = PNode->UsageInfo.AuthId;
            R.TcpConnections  = PNode->UsageInfo.TotalTcpConnections;
            R.TcpUploadSize   = PNode->UsageInfo.TotalTcpBytesFromClient;
            R.TcpDownloadSize = PNode->UsageInfo.TotalTcpBytesToClient;
            R.UdpConnections  = PNode->UsageInfo.TotalUdpChannels;
            R.UdpUploadSize   = PNode->UsageInfo.TotalUdpBytesFromClient;
            R.UdpDownloadSize = PNode->UsageInfo.TotalUdpBytesToClient;
            auto MSize        = WriteMessage(Buffer, Cmd_BackendTargetReport, R);

            KfkContext->KR.Post(std::to_string(R.AuditId), Buffer, MSize);
            TempFinishedList.AddTail(*PNode);
        }

        do {
            X_VAR xSpinlockGuard(SwitchListLock);
            FinishedCollectionList.GrabListTail(TempFinishedList);
            TempPostList.GrabListTail(PendingCollectionList);
        } while (false);
    }
}

std::string xAuditCollectReporter::GetAuditOutput() const {
    if (!KfkContext) {
        return {};
    }
    return KfkContext->KR.GetAuditOutput();
}
