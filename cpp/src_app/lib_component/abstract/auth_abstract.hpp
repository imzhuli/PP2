#pragma once
#include <expected>
#include <pp_common/_region.hpp>
#include <pp_common/future.hpp>

//
struct xAuthResult;
class xAuthAbstract;

struct xAuthResult {
    uint64_t    LocalAuthId;
    xCountryId  CountryId;
    xNetAddress ProxyAccessAddress;
    xNetAddress ExportAddress;
    bool        EnableTcp;
    bool        EnableUdp;
    uint32_t    BandwithLimit;
    uint32_t    ConnectionLimit;
    uint64_t    ExpireTime;

    std::string ToString() const;
};

struct xLocalUsageAudit {
    uint64_t CollectionStartTimestampMS = {};
    uint64_t CollectionEndTimestampMS   = {};
    uint64_t LocalTcpUploadSize         = {};
    uint64_t LocalTcpDownloadSize       = {};
    uint64_t LocalUdpUploadSize         = {};
    uint64_t LocalUdpDownloadSize       = {};
};

struct xAuthResultFuture : xFutureBase {
    xExpected<xAuthResult> Result = UnexpctedResult;
};

class xAuthAbstractService : xAbstract {
public:
    virtual void AcquireAuthInfo(const std::string_view AccountPassView, xAuthResultFuture & Future) = 0;
    virtual void ReleaseAuthInfo(uint64_t LocalAuthId, const xLocalUsageAudit & Audit)               = 0;
};

extern std::string CombineAccountPass(std::string_view AccountView, std::string_view PassView);