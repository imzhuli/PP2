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

    static constexpr size_t MaxServerIdCount() { return 0x040000; }
    static xServerType      ExtractServerType(uint64_t ServerId);
    static uint32_t         ExtractServerIndex(uint64_t ServerId);

private:
    uint16_t GenerateRandom();
    uint16_t GenerateCheckSum(uint32_t IdIndex, uint16_t IdRandom);

private:
    xObjectIdManager IdManager;
    uint16_t *       RandomPool = nullptr;

    xHolder<std::mt19937>                   RandomGeneratorHolder;
    std::uniform_int_distribution<uint32_t> RandomDistribution;
};
