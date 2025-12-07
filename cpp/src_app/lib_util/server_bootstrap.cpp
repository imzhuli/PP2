#include "./server_bootstrap.hpp"

#include <pp_common/service_runtime.hpp>

bool xServerBootstrap::Init() {
    RuntimeAssert(ServerIdClient.Init(ServiceIoContext, ServiceEnv.DefaultLocalServerIdFilePath));
    ServerIdClient.OnServerIdUpdated = Delegate(&xServerBootstrap::InternalOnServerIdUpdated, this);
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
    for (auto & R : LocalRegisterList) {
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
        ServerListClient.Init(ServiceIoContext, { MasterServerListServerAddress });
        ServerListClient.EnableServerIdCenterUpdate(true);
        ServerListClient.OnServerListUpdated = Delegate(&xServerBootstrap::InternalOnServerListUpdated, this);
        for (auto & P : LocalRegisterList) {
            P->UpdateMasterServierListAddress(MasterServerListServerAddress);
        }
    }
}

void xServerBootstrap::SetServerRegister(const std::vector<xServerRegisterInfo> & RL) {
    ReleaseAllRegisters();
    auto ServerId = ServerIdClient.GetLocalServerId();
    for (auto & I : RL) {
        auto P = std::make_unique<xServerListRegister>();
        RuntimeAssert(P->Init(ServiceIoContext));
        P->UpdateMasterServierListAddress(MasterServerListServerAddress);
        P->UpdateMyServiceInfo(I.ServiceType, { ServerId, I.Address });
        P->OnError = [] { Logger->F("register server error"); };
        LocalRegisterList.push_back(std::move(P));
    }
}

void xServerBootstrap::ReleaseAllRegisters() {
    for (auto & R : LocalRegisterList) {
        R->Clean();
    }
    Reset(LocalRegisterList);
}

void xServerBootstrap::InternalOnServerListUpdated(eServiceType Type, xVersion Version, const std::vector<xServiceInfo> & ServerList) {
    if (Type == eServiceType::ServerIdCenter) {
        if (ServerList.empty()) {
            ServerIdClient.SetServerAddress({});
        } else {
            RuntimeAssert(ServerList.size() == 1, "there should be at most 1 ServerIdCenter address");
            ServerIdClient.SetServerAddress(ServerList[0].Address);
        }
        return;
    }
    OnServerListUpdated(Type, Version, ServerList);
}

void xServerBootstrap::InternalOnServerIdUpdated(uint64_t NewId) {
    if (!NewId) {
        Logger->F("failed to gain valid server id");
        return;
    }
    Logger->I("OnServerIdUpdated: NewId=%" PRIu64 "", NewId);
    for (auto & P : LocalRegisterList) {
        P->UpdateMyServerId(NewId);
    }
}
