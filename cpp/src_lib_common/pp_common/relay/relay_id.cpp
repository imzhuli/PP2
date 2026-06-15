#include "./relay_id.hpp"

#include <crypto/md5.hpp>

static constexpr const char Salt[] = "xRelayIdManager!@#";

eRelayServerType GetRelayServerTypeFromId(uint64_t Id) {
    auto H32      = High32(Id);
    auto TypeBits = H32 & 0x7F000000;
    auto Type     = eRelayServerType(TypeBits >> 24);
    return Type < eRelayServerType::RELAY_TYPE_COUNT ? Type : eRelayServerType::UNSPECIFIED;
}

uint32_t GetRelayServerIndexFromId(uint64_t Id) {
    auto Index = Low32(Id);
    return Index <= xel::xObjectIdManager::MaxObjectId ? Index : 0;
}

static uint32_t GenerateHighBits(eRelayServerType Type, uint32_t Id) {
    ubyte Buffer[32];
    auto  W  = xel::xStreamWriter(Buffer);
    auto  TS = uint16_t(xel::GetTimestampUS());
    W.W(Salt, xel::Length(Salt));
    W.W1L(uint8_t(Type));
    W.W2L(TS);
    W.W4L(Id);

    auto CS = xel::Md5(W.Origin(), W.Offset());
    // printf("%s\n", xel::StrToHex(W.Origin(), W.Offset()).c_str());
    // printf("%s\n", xel::StrToHex(CS.Data(), CS.Size()).c_str());
    auto R  = xel::xStreamReader(CS.Data);

    uint32_t UT = uint32_t(0x80 | uint8_t(Type)) << 24;
    uint32_t UC = R.R4L() & 0x00FF0000;

    return UT | UC | TS;
}

static bool ValidateHighBits(uint64_t FullId) {
    auto HB = High32(FullId);
    auto Id = Low32(FullId);

    auto UT = HB >> 24;
    if (!(UT & 0x80)) {
        return false;
    }
    auto ST = UT & 0x7F;

    auto TS = uint16_t(HB);

    ubyte Buffer[32];
    auto  W = xel::xStreamWriter(Buffer);
    W.W(Salt, xel::Length(Salt));
    W.W1L(ST);
    W.W2L(TS);
    W.W4L(Id);

    auto CS = xel::Md5(W.Origin(), W.Offset());
    // printf("%s\n", xel::StrToHex(W.Origin(), W.Offset()).c_str());
    // printf("%s\n", xel::StrToHex(CS.Data(), CS.Size()).c_str());

    auto R = xel::xStreamReader(CS.Data);
    return (R.R4L() & 0x00FF0000) == (HB & 0x00FF0000);
}

xRelayIdManager::xRelayIdManager() {
    IdManager.Init();
}

xRelayIdManager::~xRelayIdManager() {
    IdManager.Clean();
}

uint64_t xRelayIdManager::Acquire(eRelayServerType Type) {
    assert(Type);

    auto Id = IdManager.Acquire();
    if (!Id) {
        return 0;
    }
    auto HB = GenerateHighBits(Type, Id);
    return (uint64_t(HB) << 32) | Id;
}

bool xRelayIdManager::Validate(uint64_t Id) {
    return ValidateHighBits(Id);
}
