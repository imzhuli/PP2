#pragma once
#include <object/object.hpp>
#include <pp_common/_.hpp>
#include <random>

class xServerIdManager {
public:
    bool Init();
    void Clean();

    uint64_t AcquireServerId(xServerType Type = 0);
    uint64_t RegainServerId(uint64_t ServerId);
    bool     ReleaseServerId(uint64_t ServerId);

    static xServerType ExtractServerType(uint64_t ServerId);
    static uint32_t    ExtractServerIndex(uint64_t ServerId);

private:
    uint32_t GenerateRandom();
    uint32_t GenerateCheckSum(uint32_t IdIndex, uint32_t IdRandom);

private:
    xObjectIdManager IdManager;
    uint32_t *       RandomPool = nullptr;

    xHolder<std::mt19937>                   RandomGeneratorHolder;
    std::uniform_int_distribution<uint32_t> RandomDistribution;
};
