#include "./relay_report.hpp"

#include "../lib_util/service_client_pool.hpp"
#include "./global.hpp"

#include <pp_common/_common.hpp>
#include <pp_protocol/command.hpp>
#include <pp_protocol/internal/relay_info.hpp>
#include <random>

static xel::xClientWrapper RelayReportClient;
static std::mt19937_64     Random64;

static std::vector<xServerInfo> RelayInfoDispatcherList;
static uint64_t                 SelectedRelayInfoServerId = 0;

static void PostRelayInfo();

void InitRelayReport() {
    SERVICE_RUNTIME_ASSERT(RelayReportClient.Init(ServiceIoContext));
    RelayReportClient.OnServerConnected = PostRelayInfo;

    auto RD = std::random_device();
    Random64.seed(RD());
}

void CleanRelayReport() {
    RelayReportClient.Clean();
}

void TickRelayReport(uint64_t NowMS) {
    RelayReportClient.Tick(NowMS);
}

void UpdateRelayInfoDispatcherServerList(std::vector<xServerInfo> && List) {
    X_VAR xScopeGuard([&List] {
        RelayInfoDispatcherList = std::move(List);
    });

    if (List.empty()) {
        SelectedRelayInfoServerId = 0;
        RelayReportClient.UpdateTarget({});
        return;
    }

    std::vector<xServerInfo> Added;
    Added.reserve(List.size());
    // compare and find new added servers:
    auto RemovedServerCount = 0;
    auto Removed            = false;
    auto OI                 = RelayInfoDispatcherList.begin();
    auto NI                 = List.begin();
    for (;;) {
        if (OI == RelayInfoDispatcherList.end()) {
            while (NI != List.end()) {
                Added.push_back(*NI++);
            }
            break;
        }
        if (NI == List.end()) {
            while (OI != RelayInfoDispatcherList.end()) {
                ++RemovedServerCount;
                Removed |= (OI++->ServerId == SelectedRelayInfoServerId);
            }
            break;
        }
        if (OI->ServerId < NI->ServerId) {  // remove some server
            ++RemovedServerCount;
            Removed |= (OI->ServerId == SelectedRelayInfoServerId);
            ++OI;
            continue;
        } else if (OI->ServerId == NI->ServerId) {
            ++OI;
            ++NI;
            continue;
        } else {  // OI->ServerId > NI->ServerId:  new server
            Added.push_back(*NI);
            ++NI;
            continue;
        }
    }

    if (Removed) {
        auto   Distribution       = std::uniform_int_distribution<size_t>(0, List.size() - 1);
        auto & Selected           = List[Distribution(Random64)];
        SelectedRelayInfoServerId = Selected.ServerId;
        RelayReportClient.UpdateTarget(Selected.Address);
        AuditLogger->I("Update RelayInfoDispatcher connection, selected dispatcher address: %s@%" PRIu64 "", Selected.Address.ToString().c_str(), SelectedRelayInfoServerId);
    } else if (Added.size()) {
        auto RemainedServerCount = RelayInfoDispatcherList.size() - RemovedServerCount;
        auto ChangeRate          = std::uniform_int_distribution<size_t>(0, 99999)(Random64);
        auto Change              = ChangeRate < (Added.size() * 100000 / (Added.size() + RemainedServerCount));
        if (Change) {
            auto & Selected           = Added[std::uniform_int_distribution<size_t>(0, Added.size() - 1)(Random64)];
            SelectedRelayInfoServerId = Selected.ServerId;
            RelayReportClient.UpdateTarget(Selected.Address);
            AuditLogger->I("Update RelayInfoDispatcher connection, selected dispatcher address: %s@%" PRIu64 "", Selected.Address.ToString().c_str(), SelectedRelayInfoServerId);
        }
    }
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