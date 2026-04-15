#pragma once
#include "./pa_future.hpp"

#include <pp_common/device.hpp>

struct xDeviceAcquire {
    xCountryId               CountryId;
    xDeviceFlag              Flag;
    xDeviceSelectionStrategy Strategy;
};

struct xDeviceAbstractService : xAbstract {
    virtual bool AcquireDevice(const xDeviceAcquire & Request, xPA_AcquireDeviceFuture & Future) = 0;
};
