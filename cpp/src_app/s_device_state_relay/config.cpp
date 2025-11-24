#include "./config.hpp"

#include <pp_common/service_runtime.hpp>

xNetAddress ServerListMasterServiceAddress;
xNetAddress BindObserverAddress;
xNetAddress BindProducerAddress;
xNetAddress ExportObserverAddress;
xNetAddress ExportProducerAddress;

void LoadConfig() {
    auto LC = RuntimeEnv.LoadConfig();
    LC.Require(ServerListMasterServiceAddress, "ServerListMasterServiceAddress");

    LC.Require(BindObserverAddress, "BindObserverAddress");
    LC.Require(BindProducerAddress, "BindProducerAddress");
    LC.Require(ExportObserverAddress, "ExportObserverAddress");
    LC.Require(ExportProducerAddress, "ExportProducerAddress");
}
