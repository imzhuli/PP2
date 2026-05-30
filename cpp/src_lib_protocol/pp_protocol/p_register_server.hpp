#pragma once
#include <pp_common/_.hpp>

struct xPP_RegisterServer : public xBinaryMessage {  // from proxy_access to relay server
public:
    void SerializeMembers() override {
        W(ServerType);        //
        W(PreviousServerId);  //
        W(ExportAddress);     //
    }

    void DeserializeMembers() override {
        R(ServerType);        //
        R(PreviousServerId);  //
        R(ExportAddress);     //
    }

    xServerType ServerType;
    xServerId   PreviousServerId;
    xNetAddress ExportAddress;
};

struct xPP_RegisterServerResp : public xBinaryMessage {  // from proxy_access to relay server
public:
    void SerializeMembers() override {
        W(NewServerId);  //
    }

    void DeserializeMembers() override {
        R(NewServerId);  //
    }

    xServerId NewServerId;
};
