#include "./reporter.hpp"

#include "../lib_util/rdkafka_wrapper.hpp"

#include <pp_protocol/command.hpp>

// static constexpr size_t MAX_AUDIT_REPORTER_COUNT = 2'0000;

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
    KfkContext = new xKfkContext();
    auto CL    = xel::xConfigLoader(ConfigFilename);

    CL.Require(KfkContext->SecurityProtocol, "SecurityProtocol");
    CL.Require(KfkContext->SaslMechanism, "SaslMechanism");
    CL.Require(KfkContext->SaslUsername, "SaslUsername");
    CL.Require(KfkContext->SaslPassword, "SaslPassword");
    CL.Require(KfkContext->BootstrapServerList, "BootstrapServerList");
    CL.Require(KfkContext->Topic, "Topic");

    return true;
}

void xTargetCollectReporter::Clean() {
    delete Steal(KfkContext);
}

void xTargetCollectReporter::PostTargetCollect(xTargetCollectNode * Node) {
}

// [[maybe_unused]]
// static void PostAuditAccoungUsage(xAuditAccountInfoNode & Info) {
//     DEBUG_LOG("ReportAccountInfo: %s", ToString(Info).c_str());

//     auto R             = xAD_BK_ReportUsageByAuditAccount();
//     R.LocalTimestampMS = GetTimestampMS();

//     auto & A  = R.AuditInfo;
//     A.AuditId = Info.AuditId;

//     A.TotalTcpCount        = Steal(Info.TotalTcpCount);
//     A.TotalTcpUploadSize   = Steal(Info.TotalTcpUploadSize);
//     A.TotalTcpDownloadSize = Steal(Info.TotalTcpDownloadSize);

//     A.TotalUdpCount        = Steal(Info.TotalUdpCount);
//     A.TotalUdpUploadSize   = Steal(Info.TotalUdpUploadSize);
//     A.TotalUdpDownloadSize = Steal(Info.TotalUdpDownloadSize);

//     ubyte Buffer[MaxPacketSize];
//     auto  MSize = WriteMessage(Buffer, Cmd_AuditUsageByAuditId, 0, R);

//     auto MsgKey = std::to_string(A.AuditId);
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