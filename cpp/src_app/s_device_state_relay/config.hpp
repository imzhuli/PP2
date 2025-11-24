#pragma once
#include <pp_common/_common.hpp>

extern xNetAddress ServerListMasterServiceAddress;
extern xNetAddress BindObserverAddress;
extern xNetAddress BindProducerAddress;
extern xNetAddress ExportObserverAddress;
extern xNetAddress ExportProducerAddress;

void LoadConfig();
