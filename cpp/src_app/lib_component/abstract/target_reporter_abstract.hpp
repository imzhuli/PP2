#pragma once

#include <pp_common/_common.hpp>
#include <pp_common/device.hpp>

struct xTargetReporterAbstractService {
    virtual void ReportTarget(uint64_t & GlobalAuthId, const xel::xNetAddress & TargetAddress, const std::string_view & TargetHost, size_t Count) = 0;
};
