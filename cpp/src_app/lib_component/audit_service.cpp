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
    if (!AuditClientPool.Init(ServiceIoContext, 3 * MAX_SMALL_SERVER_LIST_SIZE)) {
        TcpReporter.Clean();
        UdpReporter.Clean();
    }
    if (!ServerListDownloader.Init(ServerListServerAddress, LocalBindAddress.Decay())) {
        AuditClientPool.Clean();
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
    AuditClientPool.Clean();
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
    if (ServerType == ST_TARGET_COLLECTOR) {  // udp servers , simply replace the old list
        DEBUG_LOG("renew target collector server list (always use ServerListDownloader.GetServerListView())");
        return;
    }

    if (ServerType == ST_AUDIT_COLLECTOR) {
        auto OldList  = AuditServerList.Container.data();
        auto OldSize  = AuditServerList.Size;
        auto OldIndex = size_t(0);
        auto NewList  = ServerList;
        auto NewSize  = ServerListSize;
        auto NewIndex = size_t(0);
        DEBUG_LOG("renew audit collector server list");
        while (true) {
            if (NewIndex == NewSize) {
                for (; OldIndex < OldSize; ++OldIndex) {
                    auto & RemovedServerInfo = OldList[OldIndex];
                    DEBUG_LOG("remove audit collector server info: %" PRIx64 ", Address=%s", RemovedServerInfo.ServerId, RemovedServerInfo.Address.ToString().c_str());
                }
                break;
            }
            if (OldIndex == OldSize) {
                for (; NewIndex < NewSize; ++NewIndex) {
                    auto & AddedServerInfo = NewList[NewIndex];
                    DEBUG_LOG("add audit collector server info: %" PRIx64 ", Address=%s", AddedServerInfo.ServerId, AddedServerInfo.Address.ToString().c_str());
                }
                break;
            }
            auto & FromOld = OldList[OldIndex];
            auto & FromNew = NewList[NewIndex];
            if (FromOld.ServerId == FromNew.ServerId) {
                if (FromOld.Address == FromNew.Address) {
                    DEBUG_LOG("remain audit collector server info: %" PRIx64 ", Address=%s", FromOld.ServerId, FromOld.Address.ToString().c_str());
                } else {
                    DEBUG_LOG("remove audit collector server info: %" PRIx64 ", Address=%s", FromOld.ServerId, FromOld.Address.ToString().c_str());
                    DEBUG_LOG("add audit collector server info: %" PRIx64 ", Address=%s", FromNew.ServerId, FromNew.Address.ToString().c_str());
                }
                ++OldIndex;
                ++NewIndex;
            } else if (FromOld.ServerId < FromNew.ServerId) {  // remove old
                DEBUG_LOG("remove audit collector server info: %" PRIx64 ", Address=%s", FromOld.ServerId, FromOld.Address.ToString().c_str());
                ++OldIndex;
            } else {
                DEBUG_LOG("add audit collector server info: %" PRIx64 ", Address=%s", FromNew.ServerId, FromNew.Address.ToString().c_str());
                ++NewIndex;
            }
        }
        for (size_t I = 0; I < NewSize; ++I) {
            auto & SI = ServerList[I];
            auto & DI = AuditServerList.Container[I];
            DI        = SI;
        }
        AuditServerList.Size = NewSize;
    }
}

void xAuditService::ReportTarget(uint64_t GlobalAuthId, const xel::xNetAddress & TargetAddress, const std::string_view & TargetHost, size_t Count) {
    auto [ServerList, ServerListSize] = ServerListDownloader.GetServerListView(ST_TARGET_COLLECTOR);
    if (!ServerList || !ServerListSize) {
        ++Audit.NoServerReport;
        return;
    }

    auto ShortDomain   = ExtractSecondLevelDomain(TargetHost);
    auto Req           = xPP_TargetCollect();
    Req.GlobalAuthId   = GlobalAuthId;
    Req.TargetAddress  = TargetAddress;
    Req.TargetHostView = ShortDomain;
    Req.Count          = Count;

    ubyte Buffer[xel::MaxPacketSize];
    auto  RSize        = WriteMessage(Buffer, Cmd_TargetReport, 0, Req);
    auto  Hash         = Req.Hash;
    auto  SelectTarget = ServerList[Hash % ServerListSize];
    DEBUG_LOG("SelectReportTarget: %s", SelectTarget.Address.ToString().c_str());

    UdpReporter.PostData(SelectTarget.Address, Buffer, RSize);
}

void xAuditService::ReportUsage(const xAuditUsage & UsageInfo) {
    DEBUG_LOG("UsageInfo:%s", UsageInfo.ToString().c_str());
}

void xAuditService::ReportBlockAccount(const xAuditBlockAccount & BlockAccountInfo) {
}
