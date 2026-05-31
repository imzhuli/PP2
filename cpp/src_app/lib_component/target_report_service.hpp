#pragma once
#include "./abstract/target_reporter_abstract.hpp"
#include "./small_server_list_downloader.hpp"

#include <object/object.hpp>
#include <pp_common/_.hpp>
#include <random>

class xTargetReportService final : xTargetReporterAbstractService {
public:
    static std::string_view ExtractSecondLevelDomain(std::string_view Domain);

    bool Init(const xNetAddress & ServerListServerAddress, const xNetAddress & LocalBindAddress);
    void Clean();
    void Tick(uint64_t NowMS);
    auto OutputAudit() -> std::string;

    void Report(uint64_t & GlobalAuditId, const xel::xNetAddress & TargetAddress, const std::string_view & TargetHost, size_t Count) override;

private:
    xel::xUdpService           Reporter;
    xSmallServerListDownloader ServerListDownloader;

    struct xAudit {
        size_t NoServerReport = 0;
    };
    xAudit Audit;
};
