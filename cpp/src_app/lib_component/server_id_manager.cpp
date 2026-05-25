#include "./server_id_manager.hpp"

namespace {

    struct xServerIdComponent {
        uint32_t Id;
        uint16_t Random16;
        uint16_t Checksum;
    };

    struct xServerIdInternal {
        xServerType Type;
        uint32_t    ObjectId;
    };

    uint32_t MakeId(const xServerIdInternal & Internal) {
        return (static_cast<uint32_t>(Internal.Type) << 19) | Internal.ObjectId;
    }

    xServerIdComponent ExtractServerId(uint64_t ServerId) {
        ServerId &= 0x0FFFFFFF'FFFFFFFFu;
        return {
            .Id       = (uint32_t)(ServerId >> 32),
            .Random16 = (uint16_t)(ServerId >> 16),
            .Checksum = (uint16_t)(ServerId),
        };
    }

    xServerIdInternal ExtractServerIdInternal(uint32_t Id) {
        return {
            .Type     = (xServerType)(Id >> 19),
            .ObjectId = Id & 0x07FFFF,
        };
    }

    xServerId CombineServerId(xServerType Type, uint32_t ObjectId, uint16_t Random16, uint16_t Checksum) {
        return (static_cast<uint64_t>(0x08) << 60) | (static_cast<uint64_t>(Type) << 51) | (static_cast<uint64_t>(ObjectId) << 32) | (static_cast<uint64_t>(Random16) << 16) | static_cast<uint64_t>(Checksum);
    }

};  // namespace

bool xServerIdManager::Init() {

    static_assert(MaxServerIdCount() == IdManager.MaxObjectId);
    auto PoolSize = MaxServerIdCount();

    if (!IdManager.Init()) {
        return false;
    }
    auto IMC = xScopeCleaner(IdManager);

    assert(!RandomPool);
    RandomPool = new (std::nothrow) uint16_t[PoolSize];
    if (!RandomPool) {
        return false;
    }
    ZeroFill(RandomPool, PoolSize);

    std::random_device rd;
    RandomGeneratorHolder.CreateValue(rd());

    IMC.Dismiss();
    return true;
}

void xServerIdManager::Clean() {
    RandomGeneratorHolder.Destroy();
    delete[] Steal(RandomPool);
    auto IMC = xScopeCleaner(IdManager);
    return;
}

uint16_t xServerIdManager::GenerateRandom() {
    return (RandomDistribution(*RandomGeneratorHolder) ^ 0x784C6565u) | 0x01U;
}

uint16_t xServerIdManager::GenerateCheckSum(uint32_t IdIndex, uint16_t IdRandom) {
    assert(IdIndex);
    assert(IdRandom);
    auto Sum = IdIndex ^ (static_cast<uint32_t>(IdRandom) << 8);
    auto S0  = Sum >> 0;
    auto S1  = Sum >> 7;
    auto S2  = Sum >> 17;
    return uint16_t(S0 ^ S1 ^ S2);
}

uint64_t xServerIdManager::AcquireServerId(xServerType Type) {
    auto ObjectId = IdManager.Acquire();
    if (!ObjectId) {
        return 0;
    }
    auto Random16 = GenerateRandom();
    auto Checksum = GenerateCheckSum(MakeId({ Type, ObjectId }), Random16);

    RandomPool[ObjectId - 1] = Random16;
    return CombineServerId(Type, ObjectId, Random16, Checksum);
}

uint64_t xServerIdManager::RegainServerId(uint64_t ServerId) {

    auto [Id, Random16, Checksum] = ExtractServerId(ServerId);
    auto [Type, ObjectId]         = ExtractServerIdInternal(Id);

    if (!ObjectId || ObjectId > IdManager.MaxObjectId || !Random16) {
        // X_DEBUG_PRINTF("invalid server id: out of range");
        return 0;
    }
    if (Checksum != GenerateCheckSum(Id, Random16)) {
        // X_DEBUG_PRINTF("invalid server id: Checksum error");
        return 0;
    }

    // old ServerId is valid:
    auto & RR = RandomPool[ObjectId - 1];
    if (RR) {
        /**
            NOTE:
            event when RR == Random16, acquiring a new server id is a MUST,
            in order to avoid that more than one servers load a same server id.
         */
        // X_DEBUG_PRINTF("random pool slot is already taken");
        return AcquireServerId(Type);
    }

    // X_DEBUG_PRINTF("regain server id from unused slot");
    X_RUNTIME_ASSERT(IdManager.Acquire(ObjectId));
    RR = Random16;
    return ServerId;
}

bool xServerIdManager::ReleaseServerId(uint64_t ServerId) {

    auto [Id, Random16, Checksum] = ExtractServerId(ServerId);
    auto [Type, ObjectId]         = ExtractServerIdInternal(Id);

    if (!ObjectId || ObjectId > IdManager.MaxObjectId || !Random16) {
        // X_DEBUG_PRINTF("out of range");
        return false;
    }

    if (Checksum != GenerateCheckSum(Id, Random16)) {
        // X_DEBUG_PRINTF("invalid Checksum");
        return false;
    }

    auto & RR = RandomPool[ObjectId - 1];
    if (RR != Random16) {
        // X_DEBUG_PRINTF("key mismatch");
        return false;
    }
    RR = 0;
    IdManager.Release(ObjectId);
    return true;
}

xServerType xServerIdManager::ExtractServerType(uint64_t ServerId) {
    assert(ServerId);
    return (xServerType)(ServerId >> 51);
}

uint32_t xServerIdManager::ExtractServerIndex(uint64_t ServerId) {
    assert(ServerId);
    auto Index = (ServerId >> 32) & 0x0007FFFF;
    return (uint32_t)(Index - 1);
}
