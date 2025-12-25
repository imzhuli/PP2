#include "./relay_report.hpp"

#include "../lib_util/service_client_pool.hpp"
#include "./global.hpp"

#include <pp_common/_common.hpp>
#include <pp_protocol/command.hpp>
#include <pp_protocol/internal/relay_info.hpp>
#include <random>

static xel::xClientWrapper RelayReportClient;
static std::mt19937        Random32;

static void PostRelayInfo();

void InitRelayReport() {
    SERVICE_RUNTIME_ASSERT(RelayReportClient.Init(ServiceIoContext));
    RelayReportClient.OnConnected = PostRelayInfo;

    auto RD = std::random_device();
    Random32.seed(RD());
}

void CleanRelayReport() {
    RelayReportClient.Clean();
}

void TickRelayReport(uint64_t NowMS) {
    RelayReportClient.Tick(NowMS);
}

void UpdateRelayInfoDispatcherServerList(const std::vector<xServerInfo> & List) {
    if (List.empty()) {
        RelayReportClient.UpdateTarget({});
        return;
    }
    auto Distribution = std::uniform_int_distribution<size_t>(0, List.size() - 1);
    auto Selected     = Distribution(Random32);
    RelayReportClient.UpdateTarget(List[Selected].Address);
    return;
}

void PostRelayInfo() {
    auto   PP            = xPP_RelayInfoRegister();
    auto & SI            = PP.ExportRelayServerInfo;
    SI.ServerId          = Bootstrap.GetServerId();
    SI.ServerType        = eRelayServerType::DEVICE;
    SI.DevicePortAddress = ExportAddressForDevice;
    SI.ClientPortAddress = ExportAddressForProxyAccess4;

    RelayReportClient.PostMessage(Cmd_RelayInfoRegister, 0, PP);
}