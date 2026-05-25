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
    auto S0  = Sum >> 7;
    auto S1  = Sum >> 17;
    auto S2  = Sum >> 27;
    return 0x01F & (S0 ^ S1 ^ S2);
}

uint64_t xServerIdManager::AcquireServerId(xServerType Type) {
    auto Index = IdManager.Acquire();
    if (!Index) {
        return 0;
    }
    auto Id32             = Index | (uint32_t(Type) << 19);
    auto Random32         = GenerateRandom();
    RandomPool[Index - 1] = Random32;

    auto CheckSum = GenerateCheckSum(Id32, Random32);
    return (Make64(Id32, Random32) << 5) + CheckSum;
}

uint64_t xServerIdManager::RegainServerId(uint64_t ServerId) {

    auto CheckSum = ServerId & 0x1F;
    auto RawId    = ServerId >> 5;
    auto Id       = High32(RawId);
    auto Random32 = Low32(RawId);
    auto Index    = Id & 0x0007FFFF;
    auto Type     = xServerType(Id >> 19);

    if (!Index || Index > IdManager.MaxObjectId || !Random32) {
        // X_DEBUG_PRINTF("invalid server id: out of range");
        return 0;
    }
    if (CheckSum != GenerateCheckSum(Id, Random32)) {
        // X_DEBUG_PRINTF("invalid server id: checksum error");
        return 0;
    }

    // old ServerId is valid:
    auto & RR = RandomPool[Index - 1];
    if (RR) {
        // X_DEBUG_PRINTF("random pool slot is already taken");
        return AcquireServerId(Type);
    }

    // X_DEBUG_PRINTF("regain server id from unused slot");
    X_RUNTIME_ASSERT(IdManager.Acquire(Index));
    RR = Random32;
    return ServerId;
}

bool xServerIdManager::ReleaseServerId(uint64_t ServerId) {
    auto CheckSum = ServerId & 0x1F;
    auto RawId    = ServerId >> 5;
    auto Id       = High32(RawId);
    auto Random32 = Low32(RawId);
    auto Index    = Id & 0x0007FFFF;

    if (!Index || Index > IdManager.MaxObjectId) {
        // X_DEBUG_PRINTF("out of range");
        return false;
    }

    if (CheckSum != GenerateCheckSum(Id, Random32)) {
        // X_DEBUG_PRINTF("invalid checksum");
        return false;
    }

    auto & RR = RandomPool[Index - 1];
    if (RR != Random32) {
        // X_DEBUG_PRINTF("key mismatch");
        return false;
    }
    RR = 0;
    IdManager.Release(Index);
    return true;
}

xServerType xServerIdManager::ExtractServerType(uint64_t ServerId) {
    assert(ServerId);
    auto Type = ServerId >> 56;
    return (xServerType)Type;
}

uint32_t xServerIdManager::ExtractServerIndex(uint64_t ServerId) {
    assert(ServerId);
    auto Id    = ServerId >> 37;
    auto Index = Id & 0x0007FFFF;
    return Index - 1;
}
