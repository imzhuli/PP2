#pragma once
#include <expected>
#include <pp_common/_region.hpp>
#include <pp_common/future.hpp>

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

    std::string ToString() const;
};

struct xAuthResultFuture : xFutureBase {
    enum struct eErrorCode : uint16_t {
        OK      = 0,
        NO_DATA = 1,
        OOM     = 2,
    };
    using xResult  = std::expected<xAuthResult, eErrorCode>;
    xResult Result = std::unexpected(eErrorCode::NO_DATA);
};

class xAuthAbstractService : xAbstract {
public:
    virtual void Validate(const std::string_view AccountPass, xAuthResultFuture & Future) = 0;
};

extern std::string CombineAccountPass(std::string_view AccountView, std::string_view PassView);