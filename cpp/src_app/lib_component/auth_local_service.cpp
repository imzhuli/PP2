#include "./auth_local_service.hpp"

#include "./pa_future.hpp"

#include <pp_common/service_runtime.hpp>
#include <rapidcsv.hpp>
#include <system_error>

#ifndef NDEBUG
static constexpr const auto RELOAD_TIMEOUT   = 15s;
static constexpr const auto RESULE_POOL_SIZE = 20000;
#else
static constexpr const auto RELOAD_TIMEOUT   = 5 * 60s;
static constexpr const auto RESULE_POOL_SIZE = 10'0000;
#endif

std::string xAuthLocalRecord::ToString() const {
    auto OS = std::ostringstream();
    OS << "xAuthLocalRecord:" << endl;
    OS << "\tCountryId:" << CountryId << endl;
    OS << "\tEnableUdp:" << EnableUdp << endl;
    OS << "\tProxyClientAddress:" << ProxyClientAddress << endl;
    OS << "\tStaticExportAddress:" << StaticExportAddress << endl;
    return OS.str();
}

namespace fs = std::filesystem;

bool xAuthLocalService::Init(const char * AuthFileDir) {
    assert(AuthFileDir);
    if (!fs::is_directory(AuthFileDir, XR(std::error_code()))) {
        Logger->E("Failed to open AuthFileDir");
        return false;
    }
    this->AuthFileDir = AuthFileDir;

    RunState.Start();
    Reset(ReloadAuthFileThread, std::thread([this]() { ReloadAuthFile(); }));
    return true;
}

void xAuthLocalService::Clean() {
    RunState.Stop();
    ReloadAuthFileThread.join();
    Reset(ReloadAuthFileThread);
    RunState.Finish();

    Reset(AuthLocalMap);
    Reset(AuthFileDir);
}

std::string xAuthLocalService::OutputAudit() const {
    auto OS = std::ostringstream();
    OS << "xAuthLocalService:" << endl;
    OS << "\tCachedAuthInfoMapVersion:" << Audit.CachedAuthInfoMapVersion << endl;
    OS << "\tCachedAuthInfoCount:" << Audit.CachedAuthInfoCount << endl;
    return OS.str();
}

void xAuthLocalService::Validate(const std::string_view AccountPassView, xAuthResultFuture & Future) {
    DEBUG_LOG("ValidateAccountPass:%s", HexShow(AccountPassView).c_str());

    // check and update auth map;
    auto NewMap = LastReloadInfo.AuthMap.exchange(nullptr);
    if (NewMap) {
        AuthLocalMap              = std::move(*NewMap);
        Audit.CachedAuthInfoCount = AuthLocalMap.size();
        ++Audit.CachedAuthInfoMapVersion;
        delete NewMap;
    }

    Future.SetReady();
    Future.Result.emplace();
    auto & Result = *Future.Result;
    auto   Iter   = AuthLocalMap.find(AccountPassView);
    if (Iter == AuthLocalMap.end()) {
        return;
    }
    auto & LocalRecord = Iter->second;
    DEBUG_LOG("FoundAuthRecord:%s", LocalRecord.ToString().c_str());
    Result.ProxyAccessAddress = LocalRecord.ProxyClientAddress;
    Result.ExportAddress      = LocalRecord.StaticExportAddress;
    Result.EnableTcp          = LocalRecord.EnableTcp;
    Result.EnableUdp          = LocalRecord.EnableUdp;
    return;
}

static void LoadFile(xAuthLocalMap & TargetMap, const fs::path & FilePath) {
    auto   Doc      = rapidcsv::Document(FilePath.string(), rapidcsv::LabelParams(0, 0));
    size_t RowCount = Doc.GetRowCount();  // 数据行数（不含表头）

    auto AccountIndex         = Doc.GetColumnIdx("account");
    auto PasswordIndex        = Doc.GetColumnIdx("password");
    auto CountryCodeIndex     = Doc.GetColumnIdx("country_code");
    auto ProxyIpIndex         = Doc.GetColumnIdx("pa_ip");
    auto ExportAddressIndex   = Doc.GetColumnIdx("proxy_ip");
    auto UdpIndex             = Doc.GetColumnIdx("is_udp");
    auto SpeedLimitIndex      = Doc.GetColumnIdx("bandwidth_limit");
    auto ConnectionLimitIndex = Doc.GetColumnIdx("conn_limit");
    auto ExpiredTimeIndex     = Doc.GetColumnIdx("expired_time");
    auto Now                  = xel::GetUnixTimestamp();

    for (size_t Row = 0; Row < RowCount; ++Row) {
        auto ExpiredTimeLimit = Doc.GetCell<int64_t>(ExpiredTimeIndex, Row);
        if (ExpiredTimeLimit < Now) {
            DEBUG_LOG("AuthRow expired");
            continue;
        }

        auto Account         = Doc.GetCell<std::string>(AccountIndex, Row);
        auto Password        = Doc.GetCell<std::string>(PasswordIndex, Row);
        auto CountryCode     = Doc.GetCell<std::string>(CountryCodeIndex, Row);
        auto ProxyIp         = xNetAddress::Parse(Doc.GetCell<std::string>(ProxyIpIndex, Row));
        auto ExportAddress   = xNetAddress::Parse(Doc.GetCell<std::string>(ExportAddressIndex, Row));
        auto Udp             = Doc.GetCell<std::string>(UdpIndex, Row);
        auto SpeedLimit      = (uint64_t)std::strtoumax(Doc.GetCell<std::string>(SpeedLimitIndex, Row).c_str(), nullptr, 10);
        auto ConnectionLimit = (uint64_t)std::strtoumax(Doc.GetCell<std::string>(ConnectionLimitIndex, Row).c_str(), nullptr, 10);

        auto   AccountKey = CombineAccountPass(Account, Password);
        auto & Node       = TargetMap[AccountKey];

        Node.CountryId           = CountryCodeToCountryId(CountryCode.c_str());
        Node.ProxyClientAddress  = ProxyIp;
        Node.StaticExportAddress = ExportAddress;
        Node.EnableUdp           = atoi(Udp.c_str());

        (void)SpeedLimit;
        (void)ConnectionLimit;
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
            std::vector<std::filesystem::path>           FileList;
            std::vector<std::filesystem::file_time_type> FileTimestampList;
            for (const auto & Entry : fs::directory_iterator(AuthFileDir)) {  // build file list.
                if (!Entry.is_regular_file()) {
                    continue;
                }
                const fs::path & FilePath = Entry.path();
                if (ToLower(FilePath.extension()) != ".csv"s) {
                    continue;
                }
                FileList.push_back(FilePath);
                FileTimestampList.push_back(std::filesystem::last_write_time(FilePath));
            }
            // compare with the old list if any filename or last file save timestamp is different, reload all director && create new map
            bool NeedReload = false;
            do {
                if (FileList.size() != LastReloadInfo.FileList.size()) {
                    NeedReload = true;
                    break;
                }
                for (auto Index = decltype(FileList.size())(0); Index < FileList.size(); ++Index) {
                    auto & FilePath         = FileList[Index];
                    auto & FileTimestamp    = FileTimestampList[Index];
                    auto & OldFilePath      = LastReloadInfo.FileList[Index];
                    auto & OldFIleTimestamp = LastReloadInfo.FileTimestampList[Index];
                    if (FilePath != OldFilePath || FileTimestamp != OldFIleTimestamp) {
                        NeedReload = true;
                        break;
                    }
                }
            } while (false);

            if (NeedReload) {
                auto NewMap = new xAuthLocalMap();
                for (auto & F : FileList) {
                    LoadFile(*NewMap, F);
                }
                auto OldMap = LastReloadInfo.AuthMap.exchange(NewMap);
                if (OldMap) {
                    delete OldMap;
                }
                Reset(LastReloadInfo.FileList, std::move(FileList));
                Reset(LastReloadInfo.FileTimestampList, std::move(FileTimestampList));

                DEBUG_LOG("new auth file(s) loaded");
            }

        } catch (const fs::filesystem_error & e) {
            AuditLogger->E("iterate directory error：%s", e.what());
            return;
        }
    }

    // cleanup:
    do {
        if (auto OldMap = LastReloadInfo.AuthMap.exchange(nullptr)) {
            delete OldMap;
        }
        Reset(LastReloadInfo.FileList);
        Reset(LastReloadInfo.FileTimestampList);
    } while (false);
}
