#include "./auth_local.hpp"

#include "./pa_future.hpp"

#include <pp_common/service_runtime.hpp>
#include <rapidcsv.hpp>
#include <system_error>

static constexpr const auto RELOAD_TIMEOUT   = 60s;
static constexpr const auto RESULE_POOL_SIZE = 20000;

namespace fs = std::filesystem;

bool xAuthLocalService::Init(const char * AuthFileDir) {
    assert(AuthFileDir);
    if (!fs::is_directory(AuthFileDir, XR(std::error_code()))) {
        return false;
    }
    this->AuthFileDir = AuthFileDir;
    if (!ResultPool.Init(RESULE_POOL_SIZE)) {
        return false;
    }

    RunState.Start();
    Reset(ReloadAuthFileThread, std::thread([this]() { ReloadAuthFile(); }));
    return true;
}

void xAuthLocalService::Clean() {
    RunState.Stop();
    ReloadAuthFileThread.join();
    Reset(ReloadAuthFileThread);
    RunState.Finish();

    ResultPool.Clean();
    Reset(AuthFileDir);
}

void xAuthLocalService::Validate(const std::string_view Account, const std::string_view Pass, xPA_AuthFuture & Future) {
    auto ResultId = ResultPool.Acquire();
    if (ResultId) {
        Future.Result.emplace(ResultId);
    }
    Future.SetReady();
    return;
}

void xAuthLocalService::ReleaseAuthResult(uint64_t ResultId) {
    ResultPool.CheckAndRelease(ResultId);
}

static void LoadFile(xAuthLocalMap & TargetMap, const fs::path & FilePath) {
    auto   Doc      = rapidcsv::Document(FilePath.string(), rapidcsv::LabelParams(0, 0));
    size_t RowCount = Doc.GetRowCount();  // 数据行数（不含表头）

    auto AccountIndex         = Doc.GetColumnIdx("account");
    auto PasswordIndex        = Doc.GetColumnIdx("password");
    auto CountryCodeIndex     = Doc.GetColumnIdx("country_code");
    auto UdpIndex             = Doc.GetColumnIdx("is_udp");
    auto SpeedLimitIndex      = Doc.GetColumnIdx("bandwidth_limit");
    auto ConnectionLimitIndex = Doc.GetColumnIdx("conn_limit");
    auto ExpiredTimeIndex     = Doc.GetColumnIdx("expired_time");
    auto Now                  = xel::GetUnixTimestamp();

    for (size_t Row = 0; Row < RowCount; ++Row) {
        auto ExpiredTimeLimit = Doc.GetCell<int64_t>(ExpiredTimeIndex, Row);
        if (ExpiredTimeLimit < Now) {
            Logger->I("AuthRow expired");
            continue;
        }

        auto Account         = Doc.GetCell<std::string>(AccountIndex, Row);
        auto Password        = Doc.GetCell<std::string>(PasswordIndex, Row);
        auto CountryCode     = Doc.GetCell<std::string>(CountryCodeIndex, Row);
        auto Udp             = Doc.GetCell<std::string>(UdpIndex, Row);
        auto SpeedLimit      = (uint64_t)std::strtoumax(Doc.GetCell<std::string>(SpeedLimitIndex, Row).c_str(), nullptr, 10);
        auto ConnectionLimit = (uint64_t)std::strtoumax(Doc.GetCell<std::string>(ConnectionLimitIndex, Row).c_str(), nullptr, 10);

        auto   AccountKey = CombineAccountPass(Account, Password);
        auto & Node       = TargetMap[AccountKey];

        Node.CountryId = CountryCodeToCountryId(CountryCode.c_str());
        Node.EnableUdp = atoi(Udp.c_str());
        cout << AccountKey << endl;
        cout << CountryCode << endl;
        cout << Udp << endl;
        cout << SpeedLimit << endl;
        cout << ConnectionLimit << endl;
    }
}

void xAuthLocalService::ReloadAuthFile() {
    auto ReloadTimer = xTimer(ZeroInit);
    while (RunState) {
        if (!ReloadTimer.TestAndTag(RELOAD_TIMEOUT)) {
            std::this_thread::sleep_for(1s);
            continue;
        }

        try {
            auto NewMap = xAuthLocalMap();

            // 2. 遍历目录（非递归，只遍历当前目录）
            for (const auto & Entry : fs::directory_iterator(AuthFileDir)) {
                // 跳过子目录，只处理文件
                if (!Entry.is_regular_file()) {
                    continue;
                }
                // 获取文件路径
                const fs::path & FilePath = Entry.path();
                if (ToLower(FilePath.extension()) != ".csv"s) {
                    continue;
                }
                LoadFile(NewMap, FilePath);
            }
        } catch (const fs::filesystem_error & e) {
            // 异常处理：目录不存在、权限不足等
            AuditLogger->E("遍历目录失败：%s", e.what());
            return;
        }
    }
}
