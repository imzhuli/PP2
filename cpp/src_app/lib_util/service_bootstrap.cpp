#include "./service_bootstrap.hpp"

#include <pp_common/service_runtime.hpp>

bool xServiceBootstrap::Init(xIoContext * ICP, const xNetAddress & MasterServerListServiceAddress) {
    this->ICP                            = ICP;
    this->MasterServerListServiceAddress = MasterServerListServiceAddress;

    RuntimeAssert(ServerListClient.Init(ICP, { MasterServerListServiceAddress }));
    RuntimeAssert(ServerIdClient.Init(ICP));

    ServerListClient.EnableServerIdCenterUpdate(true);
    ServerListClient.OnServerListUpdated = Delegate(&xServiceBootstrap::InternalOnServerInfoListUpdated, this);

    ServerIdClient.OnServerIdUpdated = Delegate(&xServiceBootstrap::InternalOnServerIdUpdated, this);

    return true;
}

void xServiceBootstrap::Clean() {
    for (auto & U : ServiceRegistrationClientList) {
        U->Clean();
    }
    Reset(ServiceRegistrationClientList);
    Reset(ServiceLocalRegistration);
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
    for (auto & N : ServiceRegistrationClientList) {
        N->Tick(NowMS);
    }
}

void xServiceBootstrap::InternalOnServerIdUpdated(uint64_t ServerId) {
    for (auto & U : ServiceRegistrationClientList) {
        U->Clean();
    }
    Reset(ServiceRegistrationClientList);

    if (ServerId) {
        for (auto & N : ServiceLocalRegistration) {
            ServiceRegistrationClientList.emplace_back(new xServerListRegister());
            auto & U = ServiceRegistrationClientList.back();
            SERVICE_RUNTIME_ASSERT(U->Init(ICP));
            U->UpdateMasterServierListAddress(MasterServerListServiceAddress);
            U->UpdateMyServiceInfo(N.ServiceType, { ServerId, N.ServerAddress });
        }
    }

    OnServerIdUpdated(ServerId);
}

void xServiceBootstrap::AddServiceRegistration(eServiceType ServiceType, const xNetAddress & ServerAddress) {
    ServiceLocalRegistration.push_back({ ServiceType, ServerAddress });
    if (auto ServerId = ServerIdClient.GetLocalServerId()) {
        ServiceRegistrationClientList.emplace_back(new xServerListRegister());
        auto & U = ServiceRegistrationClientList.back();
        U->Init(ICP);
        U->UpdateMasterServierListAddress(MasterServerListServiceAddress);
        U->UpdateMyServiceInfo(ServiceType, { ServerId, ServerAddress });
    }
}

void xServiceBootstrap::RemoveServiceRegistration(eServiceType ServiceType, const xNetAddress & ServerAddress) {
    auto RemoveReference = xServiceRegistration{ ServiceType, ServerAddress };
    for (auto I = ServiceLocalRegistration.begin(); I != ServiceLocalRegistration.end(); ++I) {
        auto & N = *I;
        if (N == RemoveReference) {
            ServiceLocalRegistration.erase(I);
            break;
        }
    }
    for (auto I = ServiceRegistrationClientList.begin(); I != ServiceRegistrationClientList.end(); ++I) {
        auto & N = *I;
        if (N->GetMyServiceType() == ServiceType && N->GetMyServiceInfo().Address == ServerAddress) {
            N->Clean();
            ServiceRegistrationClientList.erase(I);
            break;
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
    if (ServiceType == eServiceType::ServerList) {
        Logger->F("list of server-list-service updated, not implemented");
        return;
    }

    OnServerInfoListUpdated(ServiceType, ServiceVersion, ServerInfoList);
}
