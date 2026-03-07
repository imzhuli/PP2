#include "./local_relay_info_manager.hpp"

#include <pp_common/relay/relay_id.hpp>

bool xLocalRelayInfoManager::Init() {
    TotalLocalRelayInfoMap = new (std::nothrow) xLocalRelayInfoList[3];
    if (!TotalLocalRelayInfoMap) {
        return false;
    }
    return true;
}

void xLocalRelayInfoManager::Clean() {
    delete[] Steal(TotalLocalRelayInfoMap);
}

bool xLocalRelayInfoManager::AddRelayInfo(const xAbstractRelayServerInfo & RelayServerInfo) {
    X_VAR NoReentry.Scope();

    auto Type  = GetRelayServerTypeFromId(RelayServerInfo.Id);
    auto Index = GetRelayServerIndexFromId(RelayServerInfo.Id);
    if (IsUnspecified(Type) || !Index) {
        Logger->E("InsertRelayInfo invalid server id");
        return false;
    }
    auto   List = TotalLocalRelayInfoMap[(size_t)Type - 1];
    auto & Ref  = List[Index];

    auto Unchanged = (RelayServerInfo == Ref.RelayServerInfo);
    if (Unchanged) {
        return true;
    }

    auto RemoveFirst = Ref.RelayServerInfo.Id;  // old value exists
    if (RemoveFirst) {
        OnRemoveRelayInfo(Ref.RelayServerInfo);
    }

    Ref.RelayServerInfo = RelayServerInfo;
    OnAddRelayInfo(Ref.RelayServerInfo);
    return true;
}

void xLocalRelayInfoManager::RemoveRelayInfo(uint64_t RelayServerId) {
    X_VAR NoReentry.Scope();

    auto Type  = GetRelayServerTypeFromId(RelayServerId);
    auto Index = GetRelayServerIndexFromId(RelayServerId);
    if (IsUnspecified(Type) || !Index) {
        Logger->E("RemoveRelayInfo: invalid relay server id");
        return;
    }
    auto   List = TotalLocalRelayInfoMap[(size_t)Type - 1];
    auto & Ref  = List[Index];
    if (Ref.RelayServerInfo.Id != RelayServerId) {
        Logger->E("RemoveRelayInfo: ServerId mismatch");
        return;
    }

    OnRemoveRelayInfo(Ref.RelayServerInfo);
    ResetLocalRelayInfoNode(Ref);
}

auto xLocalRelayInfoManager::GetLocalRelayInfo(uint64_t RelayServerId) const -> const xLocalRelayInfo * {
    auto Type  = GetRelayServerTypeFromId(RelayServerId);
    auto Index = GetRelayServerIndexFromId(RelayServerId);
    if (IsUnspecified(Type) || !Index) {
        Logger->E("GetLocalRelayInfo: invalid relay server id");
        return nullptr;
    }
    auto   List = TotalLocalRelayInfoMap[(size_t)Type - 1];
    auto & Ref  = List[Index];
    if (!Ref.RelayServerInfo.Id) {
        return nullptr;
    }
    return &Ref;
}

void xLocalRelayInfoManager::ResetLocalRelayInfoNode(xLocalRelayInfo & RelayInfo) {
    xTimeoutList::Remove(RelayInfo);
    Reset(RelayInfo);
}
