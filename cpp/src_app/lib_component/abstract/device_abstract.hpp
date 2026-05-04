#pragma once
#include <pp_common/_region.hpp>
#include <pp_common/device.hpp>
#include <pp_common/future.hpp>

struct xAcquireDeviceFuture : xFutureBase {
    struct xPA_ClientConnection * ClientConnection = nullptr;
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
    virtual bool AcquireDevice(const xDeviceRequest & Request, xAcquireDeviceFuture & Future) = 0;
};
