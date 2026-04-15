#pragma once
#include <pp_common/_common.hpp>
#include <pp_common/_region.hpp>
#include <pp_common/device.hpp>
#include <pp_common/future.hpp>

struct xDeviceAcquireFuture : xFutureBase {
    xDeviceLocator DeviceLocator;
};
