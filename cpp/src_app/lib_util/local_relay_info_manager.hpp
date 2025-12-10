#pragma once
#include <pp_common/_common.hpp>
#include <pp_common/service_runtime.hpp>

class xLocalRelayInfoManager {
private:
    struct xLocalRelayInfo {
        uint64_t               SourceId;  // where the relay_info came from
        xExportRelayServerInfo RelayServerInfo;
    };

public:
    bool Init();
    void Clean();

    bool AddRelayInfo(const xExportRelayServerInfo & RelayServerInfo, uint64_t SourceId);
    void RemoveRelayInfo(uint64_t RelayServerId);
    auto GetLocalRelayInfo(uint64_t RelayServerId) const -> const xLocalRelayInfo *;

    using xOnAddRelayInfo    = std::function<void(const xExportRelayServerInfo & RelayServerInfo)>;
    using xOnRemoveRelayInfo = std::function<void(const xExportRelayServerInfo & RelayServerInfo, uint64_t LastInfoSourceId)>;

    xOnAddRelayInfo    OnAddRelayInfo    = Noop<>;
    xOnRemoveRelayInfo OnRemoveRelayInfo = Noop<>;

private:
    xLocalRelayInfo * LocalRelayInfoMap = nullptr;
    bool              Reentry           = false;
};
