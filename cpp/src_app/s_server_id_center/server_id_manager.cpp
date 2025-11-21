#include "./server_id_manager.hpp"

bool xServerIdManager::Init() {
    if (!IdManager.Init()) {
        return false;
    }
    auto IMC = xScopeCleaner(IdManager);

    assert(!RandomPool);
    RandomPool = new (std::nothrow) uint32_t[IdManager.MaxObjectId];
    if (!RandomPool) {
        return false;
    }
    ZeroFill(RandomPool, IdManager.MaxObjectId);

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

uint32_t xServerIdManager::GenerateRandom() {
    return (RandomDistribution(*RandomGeneratorHolder) ^ 0x784C6565u) | 0x01U;
}

uint32_t xServerIdManager::GenerateCheckSum(uint32_t IdIndex, uint32_t IdRandom) {
    assert(IdIndex);
    assert(IdRandom);
    auto Sum = IdIndex ^ IdRandom;
    auto S0  = (uint32_t)(0xFFFu & (Sum >> 0));
    auto S1  = (uint32_t)(0xFFFu & (Sum >> 12));
    auto S2  = (uint32_t)(0xFFFu & (Sum >> 24));
    return S0 ^ S1 ^ S2;
}

uint64_t xServerIdManager::AcquireServerId() {
    auto Id32 = IdManager.Acquire();
    if (!Id32) {
        return 0;
    }
    auto Random32        = GenerateRandom();
    RandomPool[Id32 - 1] = Random32;

    auto CheckSum = GenerateCheckSum(Id32, Random32);
    return (Make64(Id32, Random32) << 12) + CheckSum;
}

uint64_t xServerIdManager::RegainServerId(uint64_t ServerId) {
    if (!ServerId) {
        X_DEBUG_PRINTF("ServerId no regain is required");
        return AcquireServerId();
    }

    auto CheckSum = ServerId & 0xFFF;
    auto RawId    = ServerId >> 12;
    auto Id       = High32(RawId);
    auto Random32 = Low32(RawId);

    if (!Id || Id > IdManager.MaxObjectId || !Random32) {
        X_DEBUG_PRINTF("out of range");
        return AcquireServerId();
    }
    if (CheckSum != GenerateCheckSum(Id, Random32)) {
        X_DEBUG_PRINTF("invalid checksum");
        return AcquireServerId();
    }

    auto & RR = RandomPool[Id - 1];
    if (RR) {
        X_DEBUG_PRINTF("random pool slot is already taken");
        return AcquireServerId();
    }

    X_DEBUG_PRINTF("regain server id from unused slot");
    X_RUNTIME_ASSERT(IdManager.Acquire(Id));
    RR = Random32;
    return ServerId;
}

bool xServerIdManager::ReleaseServerId(uint64_t ServerId) {
    auto CheckSum = ServerId & 0xFFF;
    auto RawId    = ServerId >> 12;
    auto Id       = High32(RawId);
    auto Random32 = Low32(RawId);

    if (!Id || Id > IdManager.MaxObjectId) {
        X_DEBUG_PRINTF("out of range");
        return false;
    }

    if (CheckSum != GenerateCheckSum(Id, Random32)) {
        X_DEBUG_PRINTF("invalid checksum");
        return false;
    }

    auto & RR = RandomPool[Id - 1];
    if (RR != Random32) {
        X_DEBUG_PRINTF("key mismatch");
        return false;
    }
    RR = 0;
    IdManager.Release(Id);
    return true;
}
