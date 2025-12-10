#include "./local_relay_info_manager.hpp"

bool xLocalRelayInfoManager::Init() {
    LocalRelayInfoMap = new (std::nothrow) xLocalRelayInfo[xObjectIdManager::MaxObjectId + 1];
    return LocalRelayInfoMap;
}

void xLocalRelayInfoManager::Clean() {
    delete[] Steal(LocalRelayInfoMap);
}

bool xLocalRelayInfoManager::AddRelayInfo(const xExportRelayServerInfo & RelayServerInfo, uint64_t SourceId) {
    auto Index = xIndexId(RelayServerInfo.ServerId);
    if (!Index || Index > xObjectIdManager::MaxObjectId) {
        Logger->E("InsertRelayInfo invalid server id");
        return false;
    }

    SERVICE_RUNTIME_ASSERT(!Reentry);
    X_VAR xValueGuard(Reentry, true);

    auto & Ref       = LocalRelayInfoMap[Index];
    auto   Unchanged = (RelayServerInfo == Ref.RelayServerInfo);
    if (Unchanged) {
        Ref.SourceId = SourceId;
        return true;
    }

    auto RemoveFirst = Ref.RelayServerInfo.ServerId;  // old value exists
    if (RemoveFirst) {
        OnRemoveRelayInfo(Ref.RelayServerInfo, Ref.SourceId);
    }

    Ref.RelayServerInfo = RelayServerInfo;
    Ref.SourceId        = SourceId;
    OnAddRelayInfo(Ref.RelayServerInfo);
    return true;
}

void xLocalRelayInfoManager::RemoveRelayInfo(uint64_t RelayServerId) {
    auto Index = xIndexId(RelayServerId);
    if (!Index || Index > xObjectIdManager::MaxObjectId) {
        Logger->E("InsertRelayInfo invalid server id");
        return;
    }
    auto & Ref = LocalRelayInfoMap[Index];
    if (Ref.RelayServerInfo.ServerId != RelayServerId) {
        Logger->E("ServerId mismatch");
        return;
    }

    SERVICE_RUNTIME_ASSERT(!Reentry);
    X_VAR xValueGuard(Reentry, true);

    OnRemoveRelayInfo(Ref.RelayServerInfo, Ref.SourceId);
    Reset(Ref);
}

auto xLocalRelayInfoManager::GetLocalRelayInfo(uint64_t RelayServerId) const -> const xLocalRelayInfo * {
    auto Index = xIndexId(RelayServerId);
    if (!Index || Index > xObjectIdManager::MaxObjectId) {
        Logger->E("InsertRelayInfo invalid server id");
        return nullptr;
    }
    auto & Ref = LocalRelayInfoMap[Index];
    if (!Ref.RelayServerInfo.ServerId) {
        return nullptr;
    }
    return &Ref;
}