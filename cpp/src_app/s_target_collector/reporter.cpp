#include "./reporter.hpp"

#include "../lib_util/rdkafka_wrapper.hpp"

#include <pp_protocol/_backend/kfk/target_collect.hpp>
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

bool xTargetCollectReporter::Init(const std::string & ConfigFilename) {

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

void xTargetCollectReporter::Clean() {
    RunState.Stop();
    Steal(KfkThread).join();
    do {  // clear resources
        xTargetCollectList FreeCollectionList;
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

void xTargetCollectReporter::Tick(uint64_t NowMS) {
    xTargetCollectList FreeCollectionList;
    do {
        X_VAR xSpinlockGuard(SwitchListLock);
        PendingCollectionList.GrabListTail(PostCollectionList);
        FreeCollectionList.GrabListTail(FinishedCollectionList);
    } while (false);
    while (auto Node = FreeCollectionList.PopHead()) {
        NodePool.Destroy(Node);
    }
}

void xTargetCollectReporter::PostTargetCollect(uint64_t GlobalAuthId, const xel::xNetAddress & TargetAddress, const std::string_view & TargetHost, size_t Count) {
    auto Node = NodePool.Create();
    if (!Node) {
        return;
    }
    Node->GlobalAuthId  = GlobalAuthId;
    Node->TargetAddress = TargetAddress;
    Node->TargetHost    = TargetHost;
    Node->Count         = Count;
    PostCollectionList.AddTail(*Node);
}

void xTargetCollectReporter::KfkThreadFunc() {
    xTargetCollectList TempPostList;
    xTargetCollectList TempFinishedList;

    auto  NowMS = xel::GetTimestampMS();
    ubyte Buffer[MaxPacketSize];
    while (RunState) {
        assert(TempFinishedList.IsEmpty());
        do {
            X_VAR xSpinlockGuard(SwitchListLock);
        } while (false);

        while (auto PNode = TempPostList.PopHead()) {
            // do post:
            auto R              = xPPB_TargetCollect();
            R.TimeMs            = NowMS;
            R.AuditId           = PNode->GlobalAuthId;
            R.Domain            = PNode->TargetHost;
            R.IpAddress         = PNode->TargetAddress;
            R.Port              = 0;
            R.TimePeriodMs      = 1;
            R.TotalRequestCount = PNode->Count;
            auto MSize          = WriteMessage(Buffer, Cmd_TargetRport, 0, R);

            auto MsgKey = std::to_string(PNode->GlobalAuthId);
            KfkContext->KR.Post(MsgKey, Buffer, MSize);

            TempFinishedList.AddTail(*PNode);
        }

        do {
            X_VAR xSpinlockGuard(SwitchListLock);
            FinishedCollectionList.GrabListTail(TempFinishedList);
            TempPostList.GrabListTail(PendingCollectionList);
        } while (false);
    }
}

// [[maybe_unused]]
// static void PostAuditAccoungUsage(xAuditAccountInfoNode & Info) {
//     DEBUG_LOG("ReportAccountInfo: %s", ToString(Info).c_str());

//     auto R             = xAD_BK_ReportUsageByAuditAccount();
//     R.LocalTimestampMS = GetTimestampMS();

//     auto & A  = R.AuditInfo;
//     A.AuthId = Info.AuthId;

//     A.TotalTcpCount        = Steal(Info.TotalTcpCount);
//     A.TotalTcpUploadSize   = Steal(Info.TotalTcpUploadSize);
//     A.TotalTcpDownloadSize = Steal(Info.TotalTcpDownloadSize);

//     A.TotalUdpCount        = Steal(Info.TotalUdpCount);
//     A.TotalUdpUploadSize   = Steal(Info.TotalUdpUploadSize);
//     A.TotalUdpDownloadSize = Steal(Info.TotalUdpDownloadSize);

//     ubyte Buffer[MaxPacketSize];
//     auto  MSize = WriteMessage(Buffer, Cmd_AuditUsageByAuthId, 0, R);

//     auto MsgKey = std::to_string(A.AuthId);
//     KR.Post(MsgKey, Buffer, MSize);

//     DEBUG_LOG("\n%s", HexShow(Buffer, MSize).c_str());
// }

// int main(int argc, char ** argv) {
//     auto REG = xRuntimeEnvGuard(argc, argv);
//     auto CL  = RuntimeEnv.LoadConfig();

//     auto BootstrapServersOpt = ParsePythonStringArray(BootstrapServerList);
//     RuntimeAssert(BootstrapServersOpt);
//     auto KfkBootstrapServers = JoinStr(*BootstrapServersOpt, ",");
//     DEBUG_LOG("KfkBootstrapServers: %s", KfkBootstrapServers.c_str());

//     RuntimeAssert(KR.Init(
//         Topic,
//         {
//             { "security.protocol", SecurityProtocol },
//             { "sasl.mechanism", SaslMechanism },
//             { "sasl.username", SaslUsername },
//             { "sasl.password", SaslPassword },
//             { "bootstrap.servers", KfkBootstrapServers },
//         }
//     ));
//     auto KRC = xScopeCleaner(KR);

//     X_GUARD(ServerIdClient, ServiceIoContext, ServerIdCenterAddress, RuntimeEnv.DefaultLocalServerIdFilePath);
//     X_GUARD(RegisterServerClient, ServiceIoContext, ServerListRegisterAddress);

//     while (ServiceRunState) {
//         ServiceUpdateOnce(ServerIdClient, RegisterServerClient);
//     }

//     return 0;
// }