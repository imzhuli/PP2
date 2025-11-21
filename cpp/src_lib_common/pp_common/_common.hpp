#pragma once
#include "./_.hpp"

/* constant section */

static constexpr const size_t   DEFAULT_MAX_SERVER_CONNECTIONS       = 4096;
static constexpr const uint32_t MAX_DEVICE_RELAY_SERVER_SUPPORTED    = 4096;  // IdManagerMini::MaxObjectId
static constexpr const uint32_t MAX_AUTH_CACHE_SERVER_COUNT          = 75;
static constexpr const uint32_t MAX_DEVICE_STATE_RELAY_SERVER_COUNT  = 75;
static constexpr const uint32_t MAX_AUDIT_DEVICE_SERVER_COUNT        = 75;
static constexpr const uint32_t MAX_AUDIT_ACCOUNT_SERVER_COUNT       = 75;
static constexpr const uint32_t MAX_AUDIT_TARGET_SERVER_COUNT        = 75;
static constexpr const uint32_t MAX_DEVICE_SELECTOR_COUNT            = 1000;
static constexpr const uint32_t MAX_DEVICE_SELECTOR_DISPATCHER_COUNT = 75;
static constexpr const uint32_t MAX_BACKEND_SERVER_COUNT             = 75;

/* shared defines */
enum struct eServiceType : uint16_t {  // used for service list, one program can have multiple services
    Unspecified    = 0,
    ServerIdCenter = 1,
    ServerList     = 2,

    ServerTest     = 255,
    MAX_TYPE_INDEX = 256,
};
