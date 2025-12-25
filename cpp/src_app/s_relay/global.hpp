#pragma once
#include "../lib_util/server_bootstrap.hpp"

#include <pp_common/service_runtime.hpp>

extern xNetAddress MasterServerListServerAddress;

extern xNetAddress BindAddressForDevice;
extern xNetAddress ExportAddressForDevice;

extern xNetAddress BindAddressForProxyAccess4;
extern xNetAddress ExportAddressForProxyAccess4;

extern xServerBootstrap Bootstrap;
