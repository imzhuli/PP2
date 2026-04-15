#include "./pa_local_service.hpp"

#include <pp_common/service_runtime.hpp>

bool xPA_LocalBindingService::Init(const std::string & JsonConfigFile) {
    X_RUNTIME_ASSERT(ServiceRunState);
    if (!LoadConfig(JsonConfigFile)) {
        return false;
    }
    if (!TcpServer.Init(ServiceIoContext, BindAddress, this)) {
        return false;
    }
    return true;
}

void xPA_LocalBindingService::Clean() {
    TcpServer.Clean();
}

bool xPA_LocalBindingService::LoadConfig(const std::string & JsonConfigFile) {
    return false;
}
