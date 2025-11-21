#pragma once
#include <object/object.hpp>
#include <pp_common/_.hpp>
#include <random>

class xServerIdManager {
public:
    bool Init();
    void Clean();

    uint64_t AcquireServerId();
    uint64_t RegainServerId(uint64_t ServerId);
    bool     ReleaseServerId(uint64_t ServerId);

    size_t GetMaxServerId() const { return IdManager.MaxObjectId; };

private:
    uint32_t GenerateRandom();
    uint32_t GenerateCheckSum(uint32_t IdIndex, uint32_t IdRandom);

private:
    xObjectIdManager IdManager;
    uint32_t *       RandomPool = nullptr;

    xHolder<std::mt19937>                   RandomGeneratorHolder;
    std::uniform_int_distribution<uint32_t> RandomDistribution;
};
