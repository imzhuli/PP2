#pragma once
#include <pp_common/_.hpp>

struct xPP_AcquireServerId : public xBinaryMessage {  // from proxy_access to relay server
public:
    void     SerializeMembers() override { W(PreviousServerId); }
    void     DeserializeMembers() override { R(PreviousServerId); }
    uint64_t PreviousServerId;
};

struct xPP_AcquireServerIdResp : public xBinaryMessage {  // from proxy_access to relay server
public:
    void SerializeMembers() override { W(PreviousServerId, NewServerId); }
    void DeserializeMembers() override { R(PreviousServerId, NewServerId); }

    uint64_t PreviousServerId;
    uint64_t NewServerId;
};
