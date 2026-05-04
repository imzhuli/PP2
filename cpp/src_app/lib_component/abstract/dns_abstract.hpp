#pragma once
#include <pp_common/_common.hpp>
#include <pp_common/future.hpp>

struct xDnsResult {
    xNetAddress A4;
    xNetAddress A6;
};

struct xDnsReultFuture : xFutureBase {
    using xResult  = std::expected<xDnsResult, xNone>;
    xResult Result = std::unexpected(None);
};

extern xDnsReultFuture * GetDnsResultFuture(const xFutureHandle & Handle);

class xDnsAbstractService : xAbstract {
    virtual bool ResolveDns(const std::string_view & HostnameView, xDnsReultFuture & Future) = 0;
};
