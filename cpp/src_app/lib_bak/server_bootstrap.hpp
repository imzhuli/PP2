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
    bool     Init(const xNetAddress & Address);
    void     Clean();
    void     Tick(uint64_t);
    void     SetServerRegister(const std::vector<xServerRegisterInfo> & RL);
    uint64_t GetServerId() const { return MostRecentServerId; }

    void EnableServerIdCenterUpdate(bool E) {  //
        ServerListClient.EnableServerIdCenterUpdate(E);
    }
    void EnableServerListSlaveUpdate(bool E) {  //
        ServerListClient.EnableServerListSlaveUpdate(E);
    }
    void EnableRelayInfoDispatcherRelayPortUpdate(bool E) {  //
        ServerListClient.EnableRelayInfoDispatcherRelayPortUpdate(E);
    }
    void EnableRelayInfoDispatcherObserverPortUpdate(bool E) {
        ServerListClient.EnableRelayInfoDispatcherObserverPortUpdate(E);
    }
    void EnableDeviceStateRelayRelayPortUpdate(bool E) {  //
        ServerListClient.EnableDeviceStateRelayRelayPortUpdate(E);
    }
    void EnableDeviceStateRelayObserverPortUpdate(bool E) {  //
        ServerListClient.EnableDeviceStateRelayObserverPortUpdate(E);
    }

    using xOnServerListUpdated = std::function<void(eServiceType, xVersion, std::vector<xServerInfo> &&)>;
    using xOnServerIdUpdated   = std::function<void(uint64_t NewId)>;

    xOnServerIdUpdated   OnServerIdUpdated   = Noop<>;
    xOnServerListUpdated OnServerListUpdated = Noop<>;

private:
    void SetMasterServerListServerAddress(const xNetAddress & Address);
    void ReleaseAllRegisters();
    void InternalOnServerListUpdated(eServiceType, xVersion, std::vector<xServerInfo> &&);
    void InternalOnServerIdUpdated(uint64_t NewId);

private:
    uint64_t    MostRecentServerId;
    xNetAddress ServerIdServiceAddress;

    xNetAddress                                                    MasterServerListServerAddress;
    xServerListClient                                              ServerListClient;
    xServerIdClient                                                ServerIdClient;
    xAutoHolder<std::vector<std::unique_ptr<xServerListRegister>>> LocalRegisterList;
};
