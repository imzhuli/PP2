#pragma once
#include <pp_common/_.hpp>

struct xPP_EchoTest : public xBinaryMessage {  // from proxy_access to relay server
public:
    void SerializeMembers() override {
        W(Data);  //
    }

    void DeserializeMembers() override {
        R(Data);  //
    }

    std::string Data;
};

using xPP_EchoTestResp = xPP_EchoTest;
