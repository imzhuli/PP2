#pragma once
#include <object/object.hpp>
#include <pp_common/_.hpp>
#include <random>

class xSmallServerListDownloader final {
    using xOnServerListUpdated = std::function<void(const xServerInfo * ServerList, size_t ServerListSize, uint64_t VersionTimestampMS)>;

public:
    bool Init(const xNetAddress & ServerListServerMasterAddress, const xNetAddress & LocalBindAddress);
    void Clean();
    void Tick(uint64_t NowMS);

    void EnableServerType(xServerType Type);
    void DisableServerType(xServerType Type);

    struct xServerListView {
        const xServerInfo * ServerList     = nullptr;
        size_t              ServerListSize = 0;
    };
    xServerListView      GetServerListView(xServerType Type);
    xOnServerListUpdated OnServerListUpdated = Noop<>;

private:
    const xNetAddress & SelectServerListServer();

    void UpdateServerListSlaveList();
    void UpdateEnabledServerList();
    void OnUdpPacket(const xUdpServiceChannelHandle &, xPacketCommandId, xPacketRequestId, ubyte *, size_t);

private:
    using xSmallServerList = std::array<xServerInfo, MAX_SMALL_SERVER_LIST_SIZE>;
    struct xEnabledServerTypeNode : xListNode {
        bool               Enabled               = false;
        xServerType        ServerType            = 0;
        uint64_t           VersionTimestampMS    = 0;
        uint64_t           LastUpdateTimestampMS = 0;
        size_t             ServerListSize        = 0;
        xSmallServerList * ServerList            = nullptr;
    };
    using xEnabledServerTypeList = xList<xEnabledServerTypeNode>;
    using xEnabledServerTypeMap  = std::array<xEnabledServerTypeNode, MAX_SMALL_SERVER_LIST_SIZE>;

    xel::xTicker                  LocalTicker;
    xel::xNetAddress              ServerListServerMasterAddress;
    xel::xUdpService              DownloadService;
    std::vector<xel::xNetAddress> ServerListServerList;
    //
    xEnabledServerTypeList        EnabledServerTypeList;
    xEnabledServerTypeMap         EnabledServerTypeMap;
    //
    xHolder<std::mt19937>         RandomGeneratorHolder;
    uint64_t                      LastUpdateServerListSlaveListTimestampMS = 0;
    xSmallServerList              TempSmallServerListForResponse;
    //
};
