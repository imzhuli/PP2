#pragma once
#include <pp_common/_common.hpp>

struct xPP_RelayInfoRegister : public xel::xBinaryMessage {

    void SerializeMembers() override {
        W(ExportRelayServerInfo.ServerId, ExportRelayServerInfo.DevicePortAddress, ExportRelayServerInfo.ClientPortAddress);
    }

    void DeserializeMembers() override {
        R(ExportRelayServerInfo.ServerId, ExportRelayServerInfo.DevicePortAddress, ExportRelayServerInfo.ClientPortAddress);
    }

    xExportRelayServerInfo ExportRelayServerInfo;
};

struct xPP_BroadcastRelayInfo : public xBinaryMessage {  // from proxy_access to relay server

    void SerializeMembers() override {
        W(ExportRelayServerInfo.ServerId, ExportRelayServerInfo.DevicePortAddress, ExportRelayServerInfo.ClientPortAddress);
    }
    void DeserializeMembers() override {
        R(ExportRelayServerInfo.ServerId, ExportRelayServerInfo.DevicePortAddress, ExportRelayServerInfo.ClientPortAddress);
    }

    xExportRelayServerInfo ExportRelayServerInfo;
};
