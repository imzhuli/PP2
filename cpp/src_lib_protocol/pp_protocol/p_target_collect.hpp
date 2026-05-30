#pragma once
#include <pp_common/_.hpp>

struct xPP_TargetCollect : public xBinaryMessage {  // from proxy_access to relay server
public:
    void SerializeMembers() override {
        W(AuditId);           //
        W(TargetAddress);     //
        W(TargetHostView);    //
        W(StartTimestampMS);  //
        W(DurationMS);        //
        W(Count);             //
    }

    void DeserializeMembers() override {
        R(AuditId);           //
        R(TargetAddress);     //
        R(TargetHostView);    //
        R(StartTimestampMS);  //
        R(DurationMS);        //
        R(Count);             //
    }

    uint64_t         AuditId;  // global authid / auditid
    xNetAddress      TargetAddress;
    std::string_view TargetHostView;
    uint64_t         StartTimestampMS;
    uint64_t         DurationMS;
    uint64_t         Count = 0;
};
