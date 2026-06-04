#include "./audit_service.hpp"

#include <pp_common/service_runtime.hpp>
#include <pp_protocol/command.hpp>
#include <pp_protocol/p_target_collect.hpp>

static std::string_view ExtractSecondLevelDomain(std::string_view Domain) {
    // move port number
    // auto colon_pos = Domain.find(':');
    // if (colon_pos != std::string_view::npos) {
    //     Domain = Domain.substr(0, colon_pos);
    // }
    size_t LastDot = std::string_view::npos;
    for (size_t I = Domain.size(); I > 0;) {
        if (Domain[--I] == '.') {
            LastDot = I;
            break;
        }
    }
    if (LastDot == std::string_view::npos) {
        return Domain;
    }

    size_t SecondLastDot = std::string_view::npos;
    for (size_t I = LastDot; I > 0;) {
        if (Domain[--I] == '.') {
            SecondLastDot = I;
            break;
        }
    }
    if (SecondLastDot == std::string_view::npos) {
        return Domain;
    }
    return Domain.substr(SecondLastDot + 1);
}

bool xAuditService::Init(const xNetAddress & ServerListServerAddress, const xNetAddress & LocalBindAddress) {
    if (!TcpReporter.Init(ServiceIoContext, MAX_SMALL_SERVER_LIST_SIZE)) {
        return false;
    }
    if (!UdpReporter.Init(ServiceIoContext, LocalBindAddress)) {
        TcpReporter.Clean();
        return false;
    }
    if (!ServerListDownloader.Init(ServerListServerAddress, LocalBindAddress.Decay())) {
        TcpReporter.Clean();
        UdpReporter.Clean();
        return false;
    }
    ServerListDownloader.EnableServerType(ST_TARGET_COLLECTOR);
    ServerListDownloader.EnableServerType(ST_AUDIT_COLLECTOR);
    ServerListDownloader.OnServerListUpdated = Delegate(&xAuditService::OnServerListUpdated, this);

    return true;
}

void xAuditService::Clean() {
    ServerListDownloader.Clean();
    UdpReporter.Clean();
    TcpReporter.Clean();
}

void xAuditService::Tick(uint64_t NowMS) {
    ServerListDownloader.Tick(NowMS);
    TcpReporter.Tick(NowMS);
}

std::string xAuditService::OutputAudit() {
    auto OS = std::ostringstream();
    OS << "xAuditService" << endl;
    OS << "\tNoServerReport=" << Steal(Audit.NoServerReport) << endl;
    return OS.str();
}

void xAuditService::OnServerListUpdated(xServerType ServerType, const xServerInfo * ServerList, size_t ServerListSize, uint64_t VersionTimestampMS) {
    auto OS = std::ostringstream();
    OS << "OnServerListUpdated, Type=" << ServerType << ", VersionTimestampMS=" << VersionTimestampMS << endl;
    for (size_t I = 0; I < ServerListSize; ++I) {
        auto & SI = ServerList[I];
        OS << "\tServerId=" << SI.ServerId << ", ServerExportAddress=" << SI.Address.ToString() << endl;
    }
    DEBUG_LOG("%s", OS.str().c_str());
}

void xAuditService::ReportTarget(uint64_t & GlobalAuthId, const xel::xNetAddress & TargetAddress, const std::string_view & TargetHost, size_t Count) {
    auto [ServerList, ServerListSize] = ServerListDownloader.GetServerListView(ST_TARGET_COLLECTOR);
    if (!ServerList || !ServerListSize) {
        ++Audit.NoServerReport;
        return;
    }
    auto ShortDomain   = ExtractSecondLevelDomain(TargetHost);
    auto Req           = xPP_TargetCollect();
    Req.AuthId         = GlobalAuthId;
    Req.TargetAddress  = TargetAddress;
    Req.TargetHostView = ShortDomain;
    Req.Count          = Count;

    ubyte Buffer[xel::MaxPacketSize];
    auto  RSize        = WriteMessage(Buffer, sizeof(Buffer), Cmd_TargetRport, Req);
    auto  Hash         = Req.Hash;
    auto  SelectTarget = ServerList[ServerListSize % Hash];
    DEBUG_LOG("SelectReportTarget: %s", SelectTarget.Address.ToString().c_str());

    UdpReporter.PostData(SelectTarget.Address, Buffer, RSize);
}

void xAuditService::ReportUsage(const xAuditUsage & UsageInfo) {
}

void xAuditService::ReportBlockAccount(const xAuditBlockAccount & BlockAccountInfo) {
}
