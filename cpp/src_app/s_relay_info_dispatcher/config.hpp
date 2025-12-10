#pragma once
#include <pp_common/service_runtime.hpp>

static constexpr const size_t MAX_RELAY_CONTEXT_COUNT    = xObjectIdManager::MaxObjectId + 1;
static constexpr const size_t MAX_OBSERVER_CONTEXT_COUNT = xObjectIdManager::MaxObjectId + 1;

extern xNetAddress MasterServerListServerAddress;

extern xNetAddress RelayPortBindAddress4;
extern xNetAddress ObserverPortBindAddress4;

extern xNetAddress RelayPortExportAddress4;
extern xNetAddress ObserverExportAddress4;

void LoadConfig();
