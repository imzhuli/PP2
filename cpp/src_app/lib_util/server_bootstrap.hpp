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
    void SetServerRegister(const std::vector<xServerRegisterInfo> & RL);

private:
    void ReleaseAllRegisters();
    void OnServerIdUpdated(uint64_t NewId);

private:
    xNetAddress ServerIdServiceAddress;

    xServerIdClient                                   ServerIdClient;
    std::vector<std::unique_ptr<xServerListRegister>> LocalRegisterList;
};
