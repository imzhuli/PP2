#include "./server_bootstrap.hpp"

#include <pp_common/service_runtime.hpp>

bool xServerBootstrap::Init(const xNetAddress & Address) {
    MostRecentServerId = 0;
    if (!ServerIdClient.Init(ServiceIoContext, ServiceEnvironment.DefaultLocalServerIdFilePath)) {
        return false;
    }
    ServerIdClient.OnServerIdUpdated = Delegate(&xServerBootstrap::InternalOnServerIdUpdated, this);
    SetMasterServerListServerAddress(Address);
    return true;
}

void xServerBootstrap::Clean() {
    if (Steal(MasterServerListServerAddress)) {
        ServerListClient.Clean();
    }
    ReleaseAllRegisters();
    ServerIdClient.Clean();
}

void xServerBootstrap::Tick(uint64_t NowMS) {
    ServerIdClient.Tick(NowMS);
    if (MasterServerListServerAddress) {
        ServerListClient.Tick(NowMS);
    }
    for (auto & R : *LocalRegisterList) {
        R->Tick(NowMS);
    }
}

void xServerBootstrap::SetMasterServerListServerAddress(const xNetAddress & Address) {
    if (MasterServerListServerAddress) {
        ServerListClient.Clean();
    }
    MasterServerListServerAddress = Address;
    if (MasterServerListServerAddress) {
        Logger->I("Update master serverlilst server address: %s", MasterServerListServerAddress.ToString().c_str());
        SERVICE_RUNTIME_ASSERT(ServerListClient.Init(ServiceIoContext, { MasterServerListServerAddress }));
        ServerListClient.EnableServerIdCenterUpdate(true);
        ServerListClient.OnServerListUpdated = Delegate(&xServerBootstrap::InternalOnServerListUpdated, this);
        for (auto & P : *LocalRegisterList) {
            P->UpdateMasterServierListAddress(MasterServerListServerAddress);
        }
    }
}

void xServerBootstrap::SetServerRegister(const std::vector<xServerRegisterInfo> & RL) {
    ReleaseAllRegisters();
    auto ServerId = ServerIdClient.GetLocalServerId();
    for (auto & I : RL) {
        auto P = std::make_unique<xServerListRegister>();
        SERVICE_RUNTIME_ASSERT(P->Init(ServiceIoContext));
        P->UpdateMasterServierListAddress(MasterServerListServerAddress);
        P->UpdateMyServiceInfo(I.ServiceType, { ServerId, I.Address });
        P->OnError = [] { AuditLogger->F("register server error"); };
        LocalRegisterList->push_back(std::move(P));
    }
}

void xServerBootstrap::ReleaseAllRegisters() {
    for (auto & R : *LocalRegisterList) {
        R->Clean();
    }
    LocalRegisterList.Reset();
}

void xServerBootstrap::InternalOnServerListUpdated(eServiceType Type, xVersion Version, std::vector<xServerInfo> && ServerList) {
    if (Type == eServiceType::ServerIdCenter) {
        if (ServerList.empty()) {
            ServerIdClient.SetServerAddress({});
        } else {
            SERVICE_RUNTIME_ASSERT(ServerList.size() == 1);
            ServerIdClient.SetServerAddress(ServerList[0].Address);
        }
        return;
    }
    if (Type == eServiceType::ServerListSlave) {
        Logger->E("unimplemented ServerListSlave support");
        return Pass();
    }
    OnServerListUpdated(Type, Version, std::move(ServerList));
}

void xServerBootstrap::InternalOnServerIdUpdated(uint64_t NewId) {
    if (!(MostRecentServerId = NewId)) {
        AuditLogger->F("failed to gain valid server id");
        return;
    }
    Logger->I("OnServerIdUpdated: NewId=%" PRIu64 "", NewId);
    for (auto & P : *LocalRegisterList) {
        P->UpdateMyServerId(NewId);
    }
    OnServerIdUpdated(NewId);
}
