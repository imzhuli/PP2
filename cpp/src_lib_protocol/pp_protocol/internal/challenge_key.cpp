#include "./challenge_key.hpp"

struct xDeviceChallengePackMessage_W : xBinaryMessage {

    void SerializeMembers() override {
        W(Pack->Version);
        W(Pack->ChannelId);
        W(Pack->Tcp4Address);
        W(Pack->Tcp6Address);
        W(Pack->Udp4Address);
        W(Pack->Udp6Address);
        W(Pack->GeoInfo.CountryId);
        W(Pack->GeoInfo.StateId);
        W(Pack->GeoInfo.CityId);
    }
    void DeserializeMembers() override {}

    const xDeviceChallengePack * Pack = nullptr;
};

struct xDeviceChallengePackMessage_R : xBinaryMessage {

    void DeserializeMembers() override {
        R(Pack->Version);
        R(Pack->ChannelId);
        R(Pack->Tcp4Address);
        R(Pack->Tcp6Address);
        R(Pack->Udp4Address);
        R(Pack->Udp6Address);
        R(Pack->GeoInfo.CountryId);
        R(Pack->GeoInfo.StateId);
        R(Pack->GeoInfo.CityId);
    }

    xDeviceChallengePack * Pack = nullptr;
};

std::string MakeChallengeKey(const xDeviceChallengePack & AddressPack) {
    auto M = xDeviceChallengePackMessage_W();
    M.Pack = &AddressPack;

    ubyte  B[MaxPacketSize];
    size_t S = M.Serialize(B, sizeof(B));

    auto H = StrToHex(B, S);

    return H;
}

xDeviceChallengePack ExtractChallengeKey(const std::string & Key) {
    auto R  = xDeviceChallengePack();
    auto RM = xDeviceChallengePackMessage_R();
    RM.Pack = &R;

    auto S = xel::HexToStr(Key);
    if (!RM.Deserialize(S.data(), S.size())) {
        return {};
    }
    return R;
}
