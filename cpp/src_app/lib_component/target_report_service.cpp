#include "./target_report_service.hpp"

#include <pp_common/service_runtime.hpp>
#include <pp_protocol/command.hpp>
#include <pp_protocol/p_target_collect.hpp>

std::string_view xTargetReportService::ExtractSecondLevelDomain(std::string_view Domain) {
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

bool xTargetReportService::Init(const xNetAddress & ServerListServerAddress, const xNetAddress & LocalBindAddress) {
    if (!Reporter.Init(ServiceIoContext, LocalBindAddress)) {
        return false;
    }
    if (!ServerListDownloader.Init(ServerListServerAddress, LocalBindAddress.Decay())) {
        Reporter.Clean();
        return false;
    }
    return true;
}

void xTargetReportService::Clean() {
    ServerListDownloader.Clean();
    Reporter.Clean();
}

void xTargetReportService::Tick(uint64_t NowMS) {
    ServerListDownloader.Tick(NowMS);
}

std::string xTargetReportService::OutputAudit() {
    auto OS = std::ostringstream();
    OS << "xTargetReportService" << endl;
    OS << "\tNoServerReport=" << Steal(Audit.NoServerReport) << endl;
    return OS.str();
}

void xTargetReportService::Report(uint64_t & GlobalAuditId, const xel::xNetAddress & TargetAddress, const std::string_view & TargetHost, size_t Count) {
    auto [ServerList, ServerListSize] = ServerListDownloader.GetServerListView(ST_TARGET_COLLECTOR);
    if (!ServerList || !ServerListSize) {
        ++Audit.NoServerReport;
        return;
    }
    auto ShortDomain   = ExtractSecondLevelDomain(TargetHost);
    auto Req           = xPP_TargetCollect();
    Req.AuditId        = GlobalAuditId;
    Req.TargetAddress  = TargetAddress;
    Req.TargetHostView = ShortDomain;
    Req.Count          = Count;

    ubyte Buffer[xel::MaxPacketSize];
    auto  RSize        = WriteMessage(Buffer, sizeof(Buffer), Cmd_TargetRport, Req);
    auto  Hash         = Req.Hash;
    auto  SelectTarget = ServerList[ServerListSize % Hash];
    DEBUG_LOG("SelectReportTarget: %s", SelectTarget.Address.ToString().c_str());

    Reporter.PostData(SelectTarget.Address, Buffer, RSize);
}
