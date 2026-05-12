#pragma once
#include <expected>
#include <pp_common/_region.hpp>
#include <pp_common/future.hpp>

//
struct xAuthResult;
class xAuthAbstract;

struct xAuthResult {
    uint64_t    AuthId;
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

struct xAuthResultFuture : xFutureBase {
    xExpected<xAuthResult> Result = UnexpctedResult;
};

class xAuthAbstractService : xAbstract {
public:
    virtual void Validate(const std::string_view AccountPassView, xAuthResultFuture & Future) = 0;
};

extern std::string CombineAccountPass(std::string_view AccountView, std::string_view PassView);