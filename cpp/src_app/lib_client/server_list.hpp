#pragma once
#include <pp_common/_.hpp>
#include <pp_common/_common.hpp>

class xServerListClient {
private:
    static constexpr uint64_t MIN_UPDATE_SERVER_ID_CENTER_TIMEOUT_MS      = 60 * 60'000;
    static constexpr uint64_t MIN_UPDATE_SERVER_LIST_SLAVE_TIMEOUT_MS     = 60 * 60'000;
    static constexpr uint64_t MIN_UPDATE_RELAY_INFO_DISPATCHER_TIMEOUT_MS = 5 * 60'000;
    static constexpr uint64_t MIN_UPDATE_DEVICE_STATE_RELAY_TIMEOUT_MS    = 5 * 60'000;

    static constexpr uint64_t MIN_UPDATE_SERVER_TEST_TIMEOUT_MS = 1 * 60'000;

public:
    bool Init(xIoContext * ICP, const std::vector<xNetAddress> & InitAddresses);
    void Clean();
    void Tick(uint64_t);

    void EnableServerIdCenterUpdate(bool E) {  //
        E ? EnableDownloader(eServiceType::ServerIdCenter, MIN_UPDATE_SERVER_ID_CENTER_TIMEOUT_MS) : DisableDownloader(eServiceType::ServerIdCenter);
    }
    void EnableServerListSlaveUpdate(bool E) {  //
        E ? EnableDownloader(eServiceType::ServerListSlave, MIN_UPDATE_SERVER_LIST_SLAVE_TIMEOUT_MS) : DisableDownloader(eServiceType::ServerListSlave);
    }
    void EnableRelayInfoDispatcherRelayPortUpdate(bool E) {  //
        E ? EnableDownloader(eServiceType::RelayInfoDispatcher_RelayPort, MIN_UPDATE_RELAY_INFO_DISPATCHER_TIMEOUT_MS)
          : DisableDownloader(eServiceType::RelayInfoDispatcher_RelayPort);
    }
    void EnableRelayInfoDispatcherObserverPortUpdate(bool E) {
        E ? EnableDownloader(eServiceType::RelayInfoDispatcher_ObserverPort, MIN_UPDATE_RELAY_INFO_DISPATCHER_TIMEOUT_MS)
          : DisableDownloader(eServiceType::RelayInfoDispatcher_ObserverPort);
    }
    void EnableDeviceStateRelayRelayPortUpdate(bool E) {  //
        E ? EnableDownloader(eServiceType::DeviceStateRelay_RelayPort, MIN_UPDATE_DEVICE_STATE_RELAY_TIMEOUT_MS) : DisableDownloader(eServiceType::DeviceStateRelay_RelayPort);
    }
    void EnableDeviceStateRelayObserverPortUpdate(bool E) {  //
        E ? EnableDownloader(eServiceType::DeviceStateRelay_ObserverPort, MIN_UPDATE_DEVICE_STATE_RELAY_TIMEOUT_MS)
          : DisableDownloader(eServiceType::DeviceStateRelay_ObserverPort);
    }

    //
    void EnableServerTestUpdate(bool E) {  //
        E ? EnableDownloader(eServiceType::ServerTest, MIN_UPDATE_SERVER_TEST_TIMEOUT_MS) : DisableDownloader(eServiceType::ServerTest);
    }

    using xOnServerListUpdated = std::function<void(eServiceType, xVersion, std::vector<xServerInfo> &&)>;

    xOnServerListUpdated OnServerListUpdated = Noop<>;

private:
    void OnTargetConnected(const xClientPoolConnectionHandle & CC);
    void OnTargetClose(const xClientPoolConnectionHandle & CC);
    void OnTargetClean(const xClientPoolConnectionHandle & CC);
    bool OnTargetPacket(const xClientPoolConnectionHandle & CC, xPacketCommandId CommandId, xPacketRequestId RequestId, ubyte * PayloadPtr, size_t PayloadSize);

    bool RequestServerListByType(eServiceType Type, xVersion CurrentVersion);
    bool OnDownloadServiceListResp(ubyte * PayloadPtr, size_t PayloadSize);

private:
    xel::xClientPool ClientPool;
    uint64_t         LastTickTimestampMS = 0;

    struct xDownloader {
        eServiceType ServiceType            = eServiceType::Unspecified;
        uint64_t     RequestTimeoutMS       = 0;
        uint64_t     LastRequestTimestampMS = 0;
        xVersion     LastVersion            = 0;
    };
    std::vector<xDownloader> EnabledDownloaders;

    void EnableDownloader(eServiceType Type, uint64_t RequestTimeoutMS);
    void DisableDownloader(eServiceType Type);
    void TryDownloader(xDownloader & Downloader, uint64_t NowMS);
    void TryDownloadServerList(uint64_t NowMS);
};
