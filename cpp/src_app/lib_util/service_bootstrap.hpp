#pragma once
#include "../lib_client/server_id.hpp"
#include "../lib_client/server_list.hpp"
#include "../lib_client/server_register.hpp"

class xServiceBootstrap {

public:
    bool Init(xIoContext * ICP, const xNetAddress & MasterServerListServiceAddress);
    void Clean();
    void Tick(uint64_t NowMS);

    uint64_t GetServiceId() const { return ServerIdClient.GetLocalServerId(); }
    void     AddServiceRegistration(eServiceType ServiceType, const xNetAddress & ServerAddress);
    void     RemoveServiceRegistration(eServiceType ServiceType, const xNetAddress & ServerAddress);

    std::function<void(uint64_t ServerId)>                                                                  OnServerIdUpdated       = Noop<>;
    std::function<void(eServiceType ServiceType, xVersion ServiceVersion, const std::vector<xServiceInfo>)> OnServerInfoListUpdated = Noop<>;

private:
    void InternalTick(uint64_t NowMS);
    void InternalOnServerIdUpdated(uint64_t ServerId);
    void InternalOnServerInfoListUpdated(eServiceType, xVersion, const std::vector<xServiceInfo> & ServerInfoList);

private:
    xIoContext * ICP = nullptr;
    xNetAddress  MasterServerListServiceAddress;

    xServerListClient ServerListClient;
    xServerIdClient   ServerIdClient;

    struct xServiceRegistration {
        eServiceType ServiceType;
        xNetAddress  ServerAddress;

        auto operator<=>(const xServiceRegistration &) const = default;
    };
    std::vector<xServiceRegistration>                 ServiceLocalRegistration;
    std::vector<std::unique_ptr<xServerListRegister>> ServiceRegistrationClientList;

    uint64_t LastTickTimestampMS = 0;
};
