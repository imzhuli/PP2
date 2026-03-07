#pragma once
#include "../_common.hpp"

/**
    @brief extrct server type from server id
    @note type value overflow is check, if invalid value is detected, return eRelayServerType::Unspecified
    @return relay server type
 */
eRelayServerType GetRelayServerTypeFromId(uint64_t Id);

/**
    @brief extract index(object id) from  server id
    @note index overflow is checked,
    @return
        0 : invalid index
        other: valid index
*/
uint32_t GetRelayServerIndexFromId(uint64_t Id);

class xRelayIdManager final : xel::xNonCopyable {
public:
    xRelayIdManager();
    ~xRelayIdManager();

    uint64_t Acquire(eRelayServerType Type);
    void     Release(uint64_t Id);
    bool     Validate(uint64_t Id);

private:
    xel::xObjectIdManager IdManager;
};