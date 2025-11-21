#pragma once
#include <pp_common/_.hpp>
#include <pp_common/_common.hpp>

struct xPP_UpdateServiceInfo : public xBinaryMessage {  // from proxy_access to relay server
public:
    void SerializeMembers() override {
        W(ServiceType);
        W(ServiceInfo.ServerId);
        W(ServiceInfo.Address);
    }
    void DeserializeMembers() override {
        R(ServiceType);
        R(ServiceInfo.ServerId);
        R(ServiceInfo.Address);
    }

    eServiceType ServiceType;
    xServiceInfo ServiceInfo;
};

struct xPP_UpdateServiceInfoResp : public xBinaryMessage {  // from proxy_access to relay server
public:
    void SerializeMembers() override { W(Accepted); }
    void DeserializeMembers() override { R(Accepted); }

    bool Accepted = false;
};

struct xPP_DownloadServiceList : public xBinaryMessage {  // from proxy_access to relay server
public:
    void         SerializeMembers() override { W(ServiceType, LastVersion); }
    void         DeserializeMembers() override { R(ServiceType, LastVersion); }
    eServiceType ServiceType;
    xVersion     LastVersion;
};

struct xPP_DownloadServiceListResp : public xBinaryMessage {  // from proxy_access to relay server
public:
    void SerializeMembers() override {
        uint16_t Count = ServiceInfoList.size();
        W(ServiceType, Version);
        W(Count);
        for (auto & I : ServiceInfoList) {
            W(I.ServerId);
            W(I.Address);
        }
    }
    void DeserializeMembers() override {
        uint16_t Count = 0;
        R(ServiceType, Version);
        R(Count);
        ServiceInfoList.resize(Count);
        for (auto & I : ServiceInfoList) {
            R(I.ServerId);
            R(I.Address);
        }
    }

    eServiceType              ServiceType;
    xVersion                  Version;
    std::vector<xServiceInfo> ServiceInfoList;
};
