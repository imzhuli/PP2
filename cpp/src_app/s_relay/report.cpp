#include "./report.hpp"

static xClientWrapper ReportClient;

void InitRelayReport() {
    SERVICE_RUNTIME_ASSERT(ReportClient.Init(ServiceIoContext));
}

void CleanRelayReport() {
    ReportClient.Clean();
}

void TickRelayReport(uint64_t NowMS) {
    ReportClient.Tick(NowMS);
}

void UpdateRelayInfoDispatcherServerList(const std::vector<xServerInfo> & ServerList) {
    if (ServerList.empty()) {
        ReportClient.UpdateTarget({});
    }
    auto Random        = ServiceTicker();
    auto SelectedIndex = Random % ServerList.size();
    ReportClient.UpdateTarget(ServerList[SelectedIndex].Address);
}
