#pragma once
#include "../lib_client/server_id.hpp"
#include "../lib_client/server_list.hpp"
#include "../lib_client/server_register.hpp"

#include <pp_common/_common.hpp>
#include <pp_common/service_runtime.hpp>

struct xServerRegisterInfo {
    eServiceType ServiceType;
    xNetAddress  Address;
};

class xServerBootstrap {
public:
    bool Init();
    void Clean();
    void Tick(uint64_t);
    void SetMasterServerListServerAddress(const xNetAddress & Address);
    void SetServerRegister(const std::vector<xServerRegisterInfo> & RL);

    using xOnServerListUpdated = std::function<void(eServiceType, xVersion, const std::vector<xServiceInfo> &)>;
    xOnServerListUpdated OnServerListUpdated;

private:
    void ReleaseAllRegisters();
    void InternalOnServerListUpdated(eServiceType, xVersion, const std::vector<xServiceInfo> &);
    void InternalOnServerIdUpdated(uint64_t NewId);

private:
    xNetAddress ServerIdServiceAddress;

    xNetAddress                                       MasterServerListServerAddress;
    xServerListClient                                 ServerListClient;
    xServerIdClient                                   ServerIdClient;
    std::vector<std::unique_ptr<xServerListRegister>> LocalRegisterList;
};
