#include "./config.hpp"

xNetAddress MasterServerListServerAddress;
xNetAddress RelayPortBindAddress4;
xNetAddress ObserverPortBindAddress4;
xNetAddress RelayPortExportAddress4;
xNetAddress ObserverExportAddress4;

void LoadConfig() {
    auto CL = ServiceEnvironment.LoadConfig();
    CL.Require(MasterServerListServerAddress, "MasterServerListServerAddress");
    CL.Require(RelayPortBindAddress4, "RelayPortBindAddress4");
    CL.Require(ObserverPortBindAddress4, "ObserverPortBindAddress4");
    CL.Require(RelayPortExportAddress4, "RelayPortExportAddress4");
    CL.Require(ObserverExportAddress4, "ObserverExportAddress4");
}