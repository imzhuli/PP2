#pragma once
#include <pp_common/_.hpp>

struct xPP_GetSmallServerList : public xBinaryMessage {  // from proxy_access to relay server
public:
    void SerializeMembers() override {
        W(ServerType);  //
    }

    void DeserializeMembers() override {
        R(ServerType);  //
    }

    xServerType ServerType;
};

struct xPP_GetSmallServerListResp : public xBinaryMessage {  // from proxy_access to relay server
public:
    void SerializeMembers() override {
        assert(ServerList && ServerListSize < MAX_SMALL_SERVER_LIST_SIZE);
        W(ServerListSize);  //
        for (size_t I = 0; I < ServerListSize; ++I) {
            auto & Ref = (*ServerList)[I];
            W(Ref.ServerId);
            W(Ref.Address);
        }
    }

    void DeserializeMembers() override {
        R(ServerListSize);  //
        assert(ServerList && ServerListSize < MAX_SMALL_SERVER_LIST_SIZE);
        for (size_t I = 0; I < ServerListSize; ++I) {
            auto & Ref = (*ServerList)[I];
            R(Ref.ServerId);
            R(Ref.Address);
        }
    }

    using xSmallServerList = std::array<xServerInfo, MAX_SMALL_SERVER_LIST_SIZE>;
    uint32_t           ServerListSize;
    xSmallServerList * ServerList;
};