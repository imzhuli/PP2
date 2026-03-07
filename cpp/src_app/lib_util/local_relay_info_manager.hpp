#pragma once
#include <pp_common/_common.hpp>
#include <pp_common/service_runtime.hpp>

class xLocalRelayInfoManager {
private:
    struct xTimeoutListNode : xListNode {
        uint64_t LastUpdateTimestampMS = 0;
    };
    using xTimeoutList = xList<xTimeoutListNode>;
    struct xLocalRelayInfo : xTimeoutListNode {
        xAbstractRelayServerInfo RelayServerInfo;
    };

public:
    bool Init();
    void Clean();

    bool AddRelayInfo(const xAbstractRelayServerInfo & RelayServerInfo);
    void RemoveRelayInfo(uint64_t RelayServerId);
    auto GetLocalRelayInfo(uint64_t RelayServerId) const -> const xLocalRelayInfo *;

    using xOnAddRelayInfo    = std::function<void(const xAbstractRelayServerInfo & RelayServerInfo)>;
    using xOnRemoveRelayInfo = std::function<void(const xAbstractRelayServerInfo & RelayServerInfo)>;

    xOnAddRelayInfo    OnAddRelayInfo    = Noop<>;
    xOnRemoveRelayInfo OnRemoveRelayInfo = Noop<>;

private:
    void ResetLocalRelayInfoNode(xLocalRelayInfo & RelayInfo);

private:
    static constexpr const size_t SingleTypeRelayInfoListSize = xObjectIdManager::MaxObjectId + 1;
    using xLocalRelayInfoList                                 = xLocalRelayInfo[SingleTypeRelayInfoListSize];

    xLocalRelayInfoList * TotalLocalRelayInfoMap = nullptr;
    xTimeoutList          TimeoutRelayInfoList;

    xNoReentry NoReentry;
};
