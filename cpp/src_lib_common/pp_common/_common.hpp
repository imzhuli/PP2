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

enum struct eRelayServerType : uint8_t {
    UNSPECIFIED = 0,
    DEVICE      = 1,
    STATIC      = 2,
    THIRD       = 3,
    RELAY_TYPE_COUNT,
};

inline bool IsUnspecified(eRelayServerType Type) {
    return Type == eRelayServerType::UNSPECIFIED;
}

struct xAbstractDeviceInfo {
    uint64_t    RelayServerId;
    uint64_t    DeviceId;
    xNetAddress DeviceAddress4;
    xNetAddress DeviceAddress6;
};

struct xAbstractRelayServerInfo {
    uint64_t Id;
    uint8_t  GroupId;

    std::strong_ordering operator<=>(const xAbstractRelayServerInfo &) const = default;
};
