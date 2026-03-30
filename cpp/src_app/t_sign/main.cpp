#include <crypto/sha.hpp>
#include <pp_common/_.cpp>

int main(int argc, char ** argv) {

    auto key = "myip"s;
    auto sec = "myip"s;

    auto now  = std::to_string(GetUnixTimestamp());
    auto path = "/proxy/static/manifest.json"s;
    auto src  = key + now + path;

    auto h    = xel::HmacSha256(src.data(), src.size(), sec.data(), sec.size());
    auto sign = xel::StrToHexLower(h.Data, sizeof(h.Data));
    printf("%s\n", sign.c_str());

    auto request = "https://54.205.125.114:8002/proxy/static/manifest.json?app_key=" + key + "&timestamp=" + now + "&sign=" + sign;
    printf("%s\n", request.c_str());

    return 0;
}
