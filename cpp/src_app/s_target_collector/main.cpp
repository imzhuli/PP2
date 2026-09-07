#include "../lib_component/server_id_client.hpp"
#include "../lib_component/small_server_list_downloader.hpp"
#include "./reporter.hpp"

#include <map>
#include <pp_common/service_runtime.hpp>
#include <pp_protocol/command.hpp>
#include <pp_protocol/p_target_collect.hpp>

#ifndef NDEBUG
static constexpr const auto OUTPUT_AUDIT_TIMEOUT_MS = 1min;
#else
static constexpr const auto OUTPUT_AUDIT_TIMEOUT_MS = 15min;
#endif
static constexpr const uint64_t TARGET_REPORT_COUNTER_LOW_BOUND = 1000;
static constexpr const uint64_t TARGET_REPORT_TIMEOUT_MS        = 5 * 60'000;

// config
static auto BindAddress           = xNetAddress();
static auto ExportAddress         = xNetAddress();
static auto ServerIdServerAddress = xNetAddress();

// service
static auto CollectorService = xUdpService();
static auto ServerIdClient   = xServerIdClient();
static auto Reporter         = xTargetCollectReporter();

struct xTargetCounter : xListNode {
    uint64_t    GlobalAuthId         = 0;
    uint64_t    LastCheckTimestampMS = 0;
    std::string Key                  = {};
    xNetAddress TargetAddress        = {};
    std::string TargetHost           = {};
    uint64_t    Count                = 0;
};
static auto TargetMap  = std::map<std::string, xTargetCounter>();
static auto TargetList = xList<xTargetCounter>();

/**
 * @brief
 *
 * @param TargetCounter
 * @return true : timestamp updated
 * @return false : removed on idle
 */
static bool DoReport(xTargetCounter & TargetCounter) {
    if (!TargetCounter.Count) {
        DEBUG_LOG("REMOVE Counter cache: Key = %s", TargetCounter.Key.c_str());
        auto Iter = TargetMap.find(TargetCounter.Key);
        assert(Iter != TargetMap.end());
        TargetMap.erase(Iter);
        return false;
    }
    DEBUG_LOG("PostCollect:%" PRIu64 ", %s, %s, %zi", TargetCounter.GlobalAuthId, TargetCounter.TargetAddress.ToString().c_str(), TargetCounter.TargetHost.c_str(), (size_t)TargetCounter.Count);
    Reporter.PostTargetCollect(TargetCounter.GlobalAuthId, TargetCounter.TargetAddress, TargetCounter.TargetHost, Steal(TargetCounter.Count));
    return true;
}

static void ReportTicker(uint64_t NowMS) {
    auto TempList = xList<xTargetCounter>();
    auto Cond     = [NowMS](const xTargetCounter & TC) { return NowMS - TC.LastCheckTimestampMS >= TARGET_REPORT_TIMEOUT_MS; };
    while (auto P = TargetList.PopHead(Cond)) {
        if (DoReport(*P)) {
            P->LastCheckTimestampMS = NowMS;
            TempList.AddTail(*P);
        }
    }
    TargetList.GrabListTail(TempList);
}

void OnUdpPacket(const xUdpServiceChannelHandle & Handle, xPacketCommandId CmdId, xPacketRequestId RequestId, ubyte * Payload, size_t PayloadSize) {
    if (CmdId == Cmd_TargetReport) {
        auto Req = xPP_TargetCollect();
        if (!Req.Deserialize(Payload, PayloadSize) || !Req.GlobalAuthId) {
            DEBUG_LOG("invalid protocol of xPP_TargetCollect");
            return;
        }
        auto Key = std::to_string(Req.GlobalAuthId) + ":";
        if (Req.TargetAddress) {
            Key += Req.TargetAddress.IpToString();
        } else if (Req.TargetHostView.size()) {
            Key += Req.TargetHostView;
        } else {
            DEBUG_LOG("Invalid target address");
            return;
        }
        auto   Now           = ServiceTicker();
        auto & TargetCounter = TargetMap[Key];
        if (!TargetCounter.GlobalAuthId) {
            assert(!xListNode::IsLinked(TargetCounter));
            DEBUG_LOG("New TargetCounterNode: GlobalAuthId=%" PRIx64 "", Req.GlobalAuthId);
            TargetCounter.GlobalAuthId         = Req.GlobalAuthId;
            TargetCounter.TargetAddress        = Req.TargetAddress;
            TargetCounter.TargetHost           = Req.TargetHostView;
            TargetCounter.Key                  = Key;
            TargetCounter.LastCheckTimestampMS = Now;
            TargetList.AddTail(TargetCounter);
        }
        TargetCounter.Count += Req.Count;
    }
}

int main(int argc, char ** argv) {
    X_VAR xServiceEnvironmentGuard(argc, argv);
    auto  CL = ServiceEnvironment.LoadConfig();
    CL.Require(BindAddress, "BindAddress");
    CL.Require(ExportAddress, "ExportAddress");
    CL.Require(ServerIdServerAddress, "ServerIdServerAddress");
    X_RESOURCE_GUARD_ASSERTED(Reporter, ServiceEnvironment.DefaultConfigFilePath);

    auto ServerIdClientOptions = xServerIdClientOptions{
        .ServerGroup      = ST_TARGET_COLLECTOR,
        .PreviousServerId = 0,
        .ExportAddress    = ExportAddress,
    };
    auto ServerIdFilename = ServiceEnvironment.DefaultLocalServerIdFilePath;
    X_RESOURCE_GUARD_ASSERTED(ServerIdClient, ServiceIoContext, ServerIdClientOptions, ServerIdServerAddress, ServerIdFilename);
    X_RESOURCE_GUARD_ASSERTED(CollectorService, ServiceIoContext, BindAddress);

    CollectorService.OnPacket = OnUdpPacket;

    auto AuditOutputTimer = xTimer();
    while (ServiceRunState) {
        ServiceUpdateOnce(ServerIdClient, Reporter, ReportTicker);
        if (AuditOutputTimer.TestAndTag(OUTPUT_AUDIT_TIMEOUT_MS)) {
            AuditLogger->I("%s", Reporter.GetAuditOutput().c_str());
        }
    };

    return 0;
}
