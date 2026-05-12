#pragma once
#include <pp_common/_region.hpp>
#include <pp_common/device.hpp>
#include <pp_common/future.hpp>

struct xDeviceReference {
    uint64_t RelayServerId;
    uint64_t RelaySideDeviceId;
};

struct xAcquireDeviceFuture : xFutureBase {
    std::expected<xDeviceReference, xNone> Result = std::unexpected(None);
};

struct xDeviceRequest {
    xDeviceFlag Flag;
    uint64_t    LastDeviceId;
    union {
        xNetAddress ExportAddress;
        xGeoInfo    GeoInfo;
    } Condition;
    xDeviceSelectionStrategy Strategy;
};

struct xDeviceLocatorAbstractService : xAbstract {
    virtual void AcquireDevice(const xDeviceRequest & Request, xAcquireDeviceFuture & Future) = 0;
};
