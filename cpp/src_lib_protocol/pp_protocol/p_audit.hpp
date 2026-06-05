#pragma once
#include <pp_common/_.hpp>
#include <pp_common/_error.hpp>

struct xPP_BlockAccount final : public xBinaryMessage {  // from proxy_access to relay server
public:
    void SerializeMembers() override;
    void DeserializeMembers() override;

    uint64_t AuthId;
    uint64_t StartTimestampMS;
    uint64_t PeriodMS;
    ;
    eBlockAccountReason Reason;
};

struct xPP_AccountUsage final : public xBinaryMessage {  // from proxy_access to relay server
public:
    void SerializeMembers() override;
    void DeserializeMembers() override;

    uint64_t AuthId;
    uint64_t StartTimestampMS;
    uint64_t PeriodMS;
    ;

    uint64_t TotalTcpBytesFromClient;
    uint64_t TotalTcpBytesToClient;
    uint64_t TotalUdpBytesFromClient;
    uint64_t TotalUdpBytesToClient;
};
