#include "./_.hpp"

#include <crypto/sha.hpp>

static constexpr const char * StaicSignSalt = "!#@SFas098xc()*&";

#define O(x) OS << #x << "=" << (x) << ' '

uint32_t HashString(const char * S) {
    uint32_t H = 0;
    while (auto C = *(S++)) {
        H = H * 33 + C;
    }
    return H;
}

uint32_t HashString(const char * S, size_t Len) {
    uint32_t H = 0;
    for (size_t I = 0; I < Len; ++I) {
        H = H * 33 + S[I];
    }
    return H;
}

uint32_t HashString(const std::string_view & S) {
    return HashString(S.data(), S.size());
}

std::string AppSign(uint64_t Timestamp, const std::string & SecretKey, const void * DataPtr, size_t Size) {
    auto TS     = std::to_string(Timestamp);
    auto Source = StaicSignSalt + TS + SecretKey + std::string((const char *)DataPtr, Size);
    auto D      = xel::Sha256(Source.data(), Source.size());
    return TS + ":" + StrToHex(D.Digest, 32);
}

bool ValidateAppSign(const std::string & Sign, const std::string & SecretKey, const void * DataPtr, size_t Size) {
    auto Segs = Split(Sign, ":");
    if (Segs.size() != 2) {
        return false;
    }
    uint64_t TimestampMS = (uint64_t)std::strtoumax(Segs[0].c_str(), nullptr, 10);
    uint64_t TimeDiff    = std::abs(SignedDiff(TimestampMS, xel::GetTimestampMS()));
    if (TimeDiff >= 60'000) {
        return false;
    }
    auto Source = StaicSignSalt + Segs[0] + SecretKey + std::string((const char *)DataPtr, Size);
    auto D      = xel::Sha256(Source.data(), Source.size());

    return StrToHex(D.Digest, 32) == Segs[1];
}
