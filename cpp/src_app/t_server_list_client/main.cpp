#include "../lib_component/small_server_list_downloader.hpp"
#include "../lib_component/target_report_service.hpp"

#include <pp_common/service_runtime.hpp>
#include <pp_protocol/command.hpp>
#include <pp_protocol/p_small_server_list.hpp>

static auto TargetAddress = xNetAddress::Parse("127.0.0.1:11000");
static auto Downloader    = xSmallServerListDownloader();

static void OnServerListUpdated(const xServerInfo * ServerList, size_t ServerListSize, uint64_t VersionTimestampMS) {
    auto OS = std::ostringstream();
    OS << "OnServerListUpdated, VersionTimestampMS=" << VersionTimestampMS << endl;
    for (size_t I = 0; I < ServerListSize; ++I) {
        auto & SI = ServerList[I];
        OS << "\tServerId=" << SI.ServerId << ", ServerExportAddress=" << SI.Address.ToString() << endl;
    }
    DEBUG_LOG("%s", OS.str().c_str());
}

static void Test(std::string_view DomainView) {
    cout << DomainView << " -> " << xTargetReportService::ExtractSecondLevelDomain(DomainView) << endl;
}

int main(int argc, char ** argv) {
    Test("www.baidu.com");
    Test("Helloworld!");
    Test("baidu.com");
    Test("a.b.c.d.baidu.com");

    X_VAR xServiceEnvironmentGuard(argc, argv, ServiceConsoleLogger);

    X_RESOURCE_GUARD_ASSERTED(Downloader, TargetAddress, xNetAddress::Make4());
    Downloader.EnableServerType(ST_TARGET_COLLECTOR);
    Downloader.OnServerListUpdated = OnServerListUpdated;

    while (ServiceRunState) {
        ServiceUpdateOnce(Downloader);
    }

    return 0;
}
