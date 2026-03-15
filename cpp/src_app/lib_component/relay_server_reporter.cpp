#include "./relay_server_reporter.hpp"

#include <pp_common/server_info_ex.hpp>

static struct {

    xAbstractRelayServerInfo   Abstract;
    xRelayServerDevicePortInfo DevicePort;
    xRelayServerProxyPortInfo  ProxyPort;

} MyRelayServerInfo;

void SetRelayServerInfo(const xAbstractRelayServerInfo & Abstract) {
    MyRelayServerInfo.Abstract = Abstract;
}

void TryReportRelayServerInfo();
