#include "./backend_challenge.hpp"

#include <crypto/md5.hpp>

std::string xBackendChallenge::Sign(const std::string & Source) {
    auto r1 = xel::Md5(Source.data(), Source.size());
    auto r2 = xel::Md5(r1.Data(), r1.Size());
    return StrToHexLower(r2.Data(), r2.Size());
}

std::string xBackendChallenge::GenerateChallengeString(const std::string & AppSecret) const {
    auto temp = AppKey + std::to_string(TimestampMS) + "@45Qws85!4" + AppSecret;
    return Sign(temp);
}
