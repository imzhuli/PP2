#include <crypto/sha.hpp>
#include <pp_common/_.cpp>
#include <skyr/url.hpp>

std::string SignPath(const std::string & Key, const std::string & Secure, uint64_t Timestamp, const std::string & Path) {
    auto Src  = Key + std::to_string(Timestamp) + Path;
    auto Hash = xel::HmacSha256(Src.data(), Src.size(), Secure.data(), Secure.size());
    auto Sign = xel::StrToHexLower(Hash.Data, sizeof(Hash.Data));
    return Sign;
}

int main(int argc, char ** argv) {
    auto Now  = GetUnixTimestamp();
    auto Key  = "myip"s;
    auto Sec  = "myip"s;
    auto Path = "/proxy/static/manifest.json"s;

    auto sign = SignPath(Key, Sec, Now, Path);
    printf("%s\n", sign.c_str());

    auto request = "http://54.205.125.114:8002/proxy/static/manifest.json?app_key=" + Key + "&timestamp=" + std::to_string(Now) + "&sign=" + sign;
    printf("%s\n", request.c_str());

    auto CL = xCommandLine{
        argc,
        argv,
        {
            { 'p', "path", "path", true },
        }
    };
    auto ExPathOpt = CL["path"];
    if (!ExPathOpt) {
        return 0;
    }

    auto ExPath = *ExPathOpt;
    auto Url    = skyr::url(ExPath);
    std::cout << Url << std::endl;
    std::cout << Url.pathname() << std::endl;

    auto ExSign  = SignPath(Key, Sec, Now, Url.pathname());
    auto Request = ExPath + "?app_key=" + Key + "&timestamp=" + std::to_string(Now) + "&sign=" + ExSign;
    cout << Request << endl;

    return 0;
}
