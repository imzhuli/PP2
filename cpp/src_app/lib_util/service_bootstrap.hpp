#pragma once
#include "../lib_client/server_id.hpp"
#include "../lib_client/server_list.hpp"
#include "../lib_client/server_register.hpp"

class xServiceBootstrap {

public:
    bool Init(xIoContext * ICP, const xNetAddress & MasterServerListServiceAddress);
    void Clean();
    void Tick(uint64_t NowMS);
    void AddServiceRegistration(eServiceType ServiceType, const xServiceInfo & ServerInfo);
    void RemoveServiceRegistration(eServiceType ServiceType, const xServiceInfo & ServerInfo);

    std::function<void(uint64_t)>                                                                           OnServerIdUpdated       = Noop<>;
    std::function<void(eServiceType ServiceType, xVersion ServiceVersion, const std::vector<xServiceInfo>)> OnServerInfoListUpdated = Noop<>;

private:
    void InternalTick(uint64_t NowMS);
    void InternalOnServerInfoListUpdated(eServiceType, xVersion, const std::vector<xServiceInfo> & ServerInfoList);

private:
    xIoContext * ICP = nullptr;
    xNetAddress  MasterServerListServiceAddress;

    xServerListClient ServerListClient;
    xServerIdClient   ServerIdClient;

    std::vector<std::unique_ptr<xServerListRegister>> ServiceRegistrationList;

    uint64_t LastTickTimestampMS = 0;
};
