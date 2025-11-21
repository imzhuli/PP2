#include "./service_bootstrap.hpp"

#include <pp_common/service_runtime.hpp>

bool xServiceBootstrap::Init(xIoContext * ICP, const xNetAddress & MasterServerListServiceAddress) {
    this->ICP                            = ICP;
    this->MasterServerListServiceAddress = MasterServerListServiceAddress;

    RuntimeAssert(ServerListClient.Init(ICP, { MasterServerListServiceAddress }));
    RuntimeAssert(ServerIdClient.Init(ICP));

    ServerListClient.EnableServerIdCenterUpdate(true);
    ServerListClient.OnServerListUpdated = Delegate(&xServiceBootstrap::InternalOnServerInfoListUpdated, this);

    ServerIdClient.OnServerIdUpdated = [this](uint64_t ServerId) { OnServerIdUpdated(ServerId); };

    return true;
}

void xServiceBootstrap::Clean() {
    for (auto & U : ServiceRegistrationList) {
        U->Clean();
    }
    Reset(ServiceRegistrationList);
    ServerListClient.Clean();
    ServerIdClient.Clean();
    Reset(ICP);
}

void xServiceBootstrap::Tick(uint64_t NowMS) {
    if (NowMS - 1'000 < LastTickTimestampMS) {
        return;
    }
    LastTickTimestampMS = NowMS;
    InternalTick(NowMS);
}

void xServiceBootstrap::InternalTick(uint64_t NowMS) {
    TickAll(NowMS, ServerListClient, ServerIdClient);
    for (auto & N : ServiceRegistrationList) {
        N->Tick(NowMS);
    }
}

void xServiceBootstrap::AddServiceRegistration(eServiceType ServiceType, const xServiceInfo & ServerInfo) {
    ServiceRegistrationList.emplace_back(new xServerListRegister());
    auto N = ServiceRegistrationList.back().get();
    RuntimeAssert(N->Init(ICP));
    N->UpdateMasterServierListAddress(MasterServerListServiceAddress);
    N->UpdateMyServiceInfo(ServiceType, ServerInfo);
}

void xServiceBootstrap::RemoveServiceRegistration(eServiceType ServiceType, const xServiceInfo & ServerInfo) {
    for (auto I = ServiceRegistrationList.begin(); I != ServiceRegistrationList.end(); ++I) {
        auto & N = *I;
        if (N->GetMyServiceType() == ServiceType && N->GetMyServiceInfo() == ServerInfo) {
            N->Clean();
            ServiceRegistrationList.erase(I);
            return;
        }
    }
}

void xServiceBootstrap::InternalOnServerInfoListUpdated(eServiceType ServiceType, xVersion ServiceVersion, const std::vector<xServiceInfo> & ServerInfoList) {
    if (ServiceType == eServiceType::ServerIdCenter) {
        if (ServerInfoList.size() != 1) {
            Logger->F("invalid server id center address list size, an singleton service is expected");
            return;
        }
        ServerIdClient.SetServerAddress(ServerInfoList[0].Address);
        return;
    }
}
