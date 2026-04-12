#include <crypto/sha.hpp>
#include <pp_common/_.cpp>

std::string SignPath(const std::string & Key, const std::string & Sec, uint64_t Timestamp, const std::string & Path) {
    auto Src = Key + std::to_string(Timestamp) + Path;

    auto Hash = xel::HmacSha256(Src.data(), Src.size(), Src.data(), Src.size());
    auto Sign = xel::StrToHexLower(Hash.Data, sizeof(Hash.Data));
    printf("%s\n", Sign.c_str());

    return Sign;
}

int main(int argc, char ** argv) {

    auto Now  = GetUnixTimestamp();
    auto Key  = "myip"s;
    auto Sec  = "myip"s;
    auto Path = "/proxy/static/manifest.json"s;

    auto sign = SignPath(Key, Sec, Now, Path);
    printf("%s\n", sign.c_str());

    auto request = "https://54.205.125.114:8002/proxy/static/manifest.json?app_key=" + Key + "&timestamp=" + std::to_string(Now) + "&sign=" + sign;
    printf("%s\n", request.c_str());

    return 0;
}
