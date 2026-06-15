#include "../lib_component/abstract/audit_abstract.hpp"
#include "../lib_component/server_id_client.hpp"
#include "../lib_component/small_server_list_downloader.hpp"
#include "./reporter.hpp"

#include <pp_common/service_runtime.hpp>
#include <pp_protocol/command.hpp>
#include <pp_protocol/p_audit_collect.hpp>
#include <pp_protocol/p_block_account.hpp>

#ifndef NDEBUG
static constexpr const auto OUTPUT_AUDIT_TIMEOUT_MS = 1min;
#else
static constexpr const auto OUTPUT_AUDIT_TIMEOUT_MS = 15min;
#endif

static constexpr const size_t MAX_REPORTER_CONNECTION_COUNT = 5'0000;

// config
static auto BindAddress           = xNetAddress();
static auto ExportAddress         = xNetAddress();
static auto ServerIdServerAddress = xNetAddress();

//
static auto CollectorService = xTcpService();
static auto ServerIdClient   = xServerIdClient();
static auto Reporter         = xAuditCollectReporter();

static bool OnClientPacket(const xTcpServiceClientConnectionHandle &, xPacketCommandId CommandId, xPacketRequestId, ubyte * Payload, size_t PayloadSize) {
    if (CommandId == Cmd_AuditReport) {
        auto Req = xPP_AuditCollect();
        if (!Req.Deserialize(Payload, PayloadSize)) {
            DEBUG_LOG("invalid protocol");
            return false;
        }
        auto UsageInfo                    = xAuditUsage();
        UsageInfo.AuthId                  = Req.GlobalAuthId;
        UsageInfo.StartTimestampMS        = Req.StartTimestampMS;
        UsageInfo.PeriodMS                = Req.PeriodMS;
        UsageInfo.TotalTcpConnections     = Req.TotalTcpConnections;
        UsageInfo.TotalTcpBytesFromClient = Req.TotalTcpBytesFromClient;
        UsageInfo.TotalTcpBytesToClient   = Req.TotalTcpBytesToClient;
        UsageInfo.TotalUdpChannels        = Req.TotalUdpChannels;
        UsageInfo.TotalUdpBytesFromClient = Req.TotalUdpBytesFromClient;
        UsageInfo.TotalUdpBytesToClient   = Req.TotalUdpBytesToClient;
        Reporter.PostAuditCollect(UsageInfo);
        return true;
    }
    if (CommandId == Cmd_BlockAccountReport) {
        auto Req = xPP_BlockAccount();
        if (!Req.Deserialize(Payload, PayloadSize)) {
            DEBUG_LOG("invalid protocol");
            return false;
        }
        auto BlockAccountInfo             = xAuditBlockAccount();
        BlockAccountInfo.AuthId           = Req.GlobalAuthId;
        BlockAccountInfo.StartTimestampMS = Req.StartTimestampMS;
        BlockAccountInfo.PeriodMS         = Req.BlockPeriodMS;
        BlockAccountInfo.Reason           = Req.BlockReason;
        BlockAccountInfo.Threshold        = Req.BlockThreshold;
        BlockAccountInfo.TriggerValue     = Req.BlockTriggerValue;
        Reporter.PostBlockAccount(BlockAccountInfo);
    }
    return true;
}

int main(int argc, char ** argv) {
    X_VAR xServiceEnvironmentGuard(argc, argv);
    auto  CL = ServiceEnvironment.LoadConfig();

    CL.Require(BindAddress, "BindAddress");
    CL.Require(ExportAddress, "ExportAddress");
    CL.Require(ServerIdServerAddress, "ServerIdServerAddress");
    X_RESOURCE_GUARD_ASSERTED(Reporter, ServiceEnvironment.DefaultConfigFilePath);

    auto ServerIdClientOptions = xServerIdClientOptions{
        .ServerGroup      = ST_AUDIT_COLLECTOR,
        .PreviousServerId = 0,
        .ExportAddress    = ExportAddress,
    };
    auto ServerIdFilename = ServiceEnvironment.DefaultLocalServerIdFilePath;
    X_RESOURCE_GUARD_ASSERTED(ServerIdClient, ServiceIoContext, ServerIdClientOptions, ServerIdServerAddress, ServerIdFilename);
    X_RESOURCE_GUARD_ASSERTED(CollectorService, ServiceIoContext, BindAddress, MAX_REPORTER_CONNECTION_COUNT);

    CollectorService.OnClientPacket = &OnClientPacket;

    auto AuditOutputTimer = xTimer();
    while (ServiceRunState) {
        ServiceUpdateOnce(ServerIdClient, CollectorService, Reporter);
        if (AuditOutputTimer.TestAndTag(OUTPUT_AUDIT_TIMEOUT_MS)) {
            AuditLogger->I("%s", Reporter.GetAuditOutput().c_str());
        }
    }

    return 0;
}