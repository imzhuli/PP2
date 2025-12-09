#pragma once
#include "./_.hpp"

/* constant section */

static constexpr const uint32_t MAX_AUTH_CACHE_SERVER_COUNT            = 100;
static constexpr const uint32_t MAX_RELAY_SERVER_SUPPORTED             = 2'0000;
static constexpr const uint32_t MAX_RELAY_INFO_DISPATCHER_SERVER_COUNT = 100;
static constexpr const uint32_t MAX_DEVICE_STATE_RELAY_SERVER_COUNT    = 100;
static constexpr const uint32_t MAX_AUDIT_DEVICE_SERVER_COUNT          = 100;
static constexpr const uint32_t MAX_AUDIT_ACCOUNT_SERVER_COUNT         = 100;
static constexpr const uint32_t MAX_AUDIT_TARGET_SERVER_COUNT          = 100;
static constexpr const uint32_t MAX_DEVICE_SELECTOR_DISPATCHER_COUNT   = 100;
static constexpr const uint32_t MAX_BACKEND_SERVER_COUNT               = 100;
static constexpr const uint32_t MAX_DEVICE_SELECTOR_COUNT              = 1000;

/* shared defines */
enum struct eServiceType : uint16_t {  // used for service list, one program can have multiple services
    Unspecified     = 0,
    ServerIdCenter  = 1,
    ServerListSlave = 2,

    RelayInfoDispatcher_RelayPort    = 3,
    RelayInfoDispatcher_ObserverPort = 4,

    DeviceStateRelay_RelayPort    = 5,
    DeviceStateRelay_ObserverPort = 6,

    ServerTest     = 255,
    MAX_TYPE_INDEX = 256,
};
