#pragma once
#include "./abstract/audit_abstract.hpp"
#include "./abstract/target_reporter_abstract.hpp"
#include "./small_server_list_downloader.hpp"

#include <object/object.hpp>
#include <pp_common/_.hpp>
#include <random>

class xAuditService final
    : public xTargetReporterAbstractService
    , public xAuditAbstractService {
public:
    bool Init(const xNetAddress & ServerListServerAddress, const xNetAddress & LocalBindAddress);
    void Clean();
    void Tick(uint64_t NowMS);
    auto OutputAudit() -> std::string;

    void ReportTarget(uint64_t & GlobalAuthId, const xel::xNetAddress & TargetAddress, const std::string_view & TargetHost, size_t Count) override;
    void ReportUsage(const xAuditUsage & UsageInfo) override;
    void ReportBlockAccount(const xAuditBlockAccount & BlockAccountInfo) override;

private:
    void OnServerListUpdated(xServerType ServerType, const xServerInfo * ServerList, size_t ServerListSize, uint64_t VersionTimestampMS);

private:
    xel::xTcpClientPool        TcpReporter;
    xel::xUdpService           UdpReporter;
    xSmallServerListDownloader ServerListDownloader;

    struct xAudit {
        size_t NoServerReport = 0;
    };
    xAudit Audit;
};
