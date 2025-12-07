#include "./server_bootstrap.hpp"

#include <pp_common/service_runtime.hpp>

bool xServerBootstrap::Init() {
    RuntimeAssert(ServerIdClient.Init(ServiceIoContext, ServiceEnv.DefaultLocalServerIdFilePath));
    ServerIdClient.OnServerIdUpdated = Delegate(&xServerBootstrap::OnServerIdUpdated, this);
    return true;
}

void xServerBootstrap::Clean() {
    ReleaseAllRegisters();
}

void xServerBootstrap::Tick(uint64_t NowMS) {
    ServerIdClient.Tick(NowMS);
    for (auto & R : LocalRegisterList) {
        R->Tick(NowMS);
    }
}

void xServerBootstrap::SetServerRegister(const std::vector<xServerRegisterInfo> & RL) {
    ReleaseAllRegisters();
    auto ServerId = ServerIdClient.GetLocalServerId();
    for (auto & I : RL) {
        auto P = std::make_unique<xServerListRegister>();
        RuntimeAssert(P->Init(ServiceIoContext));
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

void xServerBootstrap::OnServerIdUpdated(uint64_t NewId) {
    if (!NewId) {
        Logger->F("failed to gain valid server id");
        return;
    }
    Logger->I("OnServerIdUpdated: NewId=%" PRIu64 "", NewId);
    for (auto & P : LocalRegisterList) {
        P->UpdateMyServerId(NewId);
    }
}
