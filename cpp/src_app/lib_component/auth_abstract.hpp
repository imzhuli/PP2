#pragma once
#include <pp_common/_common.hpp>
#include <pp_common/_region.hpp>

// external
struct xPA_AuthFuture;

//
struct xAuthResult;
class xAuthAbstract;

struct xAuthResult {
    xCountryId  CountryId;
    xNetAddress ProxyAccessAddress;
    bool        EnableUdp;
    uint32_t    BandwithLimit;
    uint32_t    ConnectionLimit;
    uint64_t    ExpireTime;
};

class xAuthAbstractService : xAbstract {
public:
    virtual void Validate(const std::string_view Account, const std::string_view Pass, xPA_AuthFuture & Future) = 0;
    virtual void ReleaseAuthResult(uint64_t ResultId)                                                           = 0;
};

extern std::string CombineAccountPass(std::string_view AccountView, std::string_view PassView);