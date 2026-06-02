#include "./auth_local_service.hpp"

#include "./pa_future.hpp"

#include <pp_common/service_runtime.hpp>
#include <rapidcsv.hpp>
#include <system_error>

namespace fs = std::filesystem;

#ifndef NDEBUG
static constexpr const auto RELOAD_TIMEOUT   = 15s;
static constexpr const auto RESULE_POOL_SIZE = 20000;
#else
static constexpr const auto RELOAD_TIMEOUT   = 5 * 60s;
static constexpr const auto RESULE_POOL_SIZE = 10'0000;
#endif

static constexpr const size_t LOCAL_AUDIT_NODE_POOL_SIZE       = 50000;
static constexpr const size_t LOCAL_AUDIT_NODE_IDLE_TIMEOUT_MS = 30'000;

std::string xAuthLocalRecord::ToString() const {
    auto OS = std::ostringstream();
    OS << "xAuthLocalRecord:" << endl;
    OS << "\tCountryId:" << CountryId << endl;
    OS << "\tEnableUdp:" << EnableUdp << endl;
    OS << "\tProxyClientAddress:" << ProxyClientAddress.IpToString() << endl;
    OS << "\tStaticExportAddress:" << StaticExportAddress.IpToString();
    return OS.str();
}

std::string xAuthLocalUsageAuditNode::ToString() const {
    auto OS = std::ostringstream();
    OS << "xAuthLocalUsageAuditNode:" << endl;
    OS << "\tLocalAuthId:" << LocalAuthId << endl;
    OS << "\tReferenceCount:" << ReferenceCount << endl;
    OS << "\tGlobalAuthId:" << GlobalAuthId << endl;
    OS << "\tLocalTcpUploadSize:" << Audit.LocalTcpUploadSize << endl;
    OS << "\tLocalTcpDownloadSize:" << Audit.LocalTcpDownloadSize << endl;
    OS << "\tLocalUdpUploadSize:" << Audit.LocalUdpUploadSize << endl;
    OS << "\tLocalUdpDownloadSize:" << Audit.LocalUdpDownloadSize << endl;

    return OS.str();
}

bool xAuthLocalService::Init(const std::string & AuthFileDir) {
    if (!LocalAuditNodePool.Init(LOCAL_AUDIT_NODE_POOL_SIZE)) {
        return false;
    }

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

    LocalAuditNodePool.Clean();
}

void xAuthLocalService::Tick(uint64_t NowMS) {
    LocalTicker.Update(NowMS);
    CheckAndResportLocalAudit();
}

void xAuthLocalService::CheckAndResportLocalAudit() {
    auto NowMS         = LocalTicker();
    auto KillTimepoint = NowMS - LOCAL_AUDIT_NODE_IDLE_TIMEOUT_MS;
    auto Cond          = [=](const xAuthLocalUsageAuditNode & N) {
        return N.TimestampMS <= KillTimepoint;
    };
    while (auto P = LocalAuditTimeoutList.PopHead(Cond)) {
        // TODO: DO REPORT HERE
        DEBUG_LOG("%s", P->ToString().c_str());

        if (P->ReferenceCount) {
            P->TimestampMS = NowMS;
            LocalAuditTimeoutList.AddTail(*P);
        } else {
            --Audit.UnReleasedAuthInfoCount;
            LocalAuditNodePool.Release(P->LocalAuthId);
        }
    }
}

std::string xAuthLocalService::OutputAudit() const {
    auto OS = std::ostringstream();
    OS << "xAuthLocalService:" << endl;
    OS << "\tUnReleasedAuthInfoCount:" << Audit.UnReleasedAuthInfoCount << endl;
    OS << "\tCachedAuthInfoMapVersion:" << Audit.CachedAuthInfoMapVersion << endl;
    OS << "\tCachedAuthInfoCount:" << Audit.CachedAuthInfoCount << endl;
    return OS.str();
}

void xAuthLocalService::AcquireAuthInfo(const std::string_view AccountPassView, xAuthResultFuture & Future) {
    DEBUG_LOG("\n%s", HexShow(AccountPassView).c_str());

    // check and update auth map;
    auto NewMap = LastReloadInfo.AuthMap.exchange(nullptr);
    if (NewMap) {
        AuthLocalMap              = std::move(*NewMap);
        Audit.CachedAuthInfoCount = AuthLocalMap.size();
        ++Audit.CachedAuthInfoMapVersion;
        delete NewMap;
    }

    Future.SetReady();
    auto Iter = AuthLocalMap.find(AccountPassView);
    if (Iter == AuthLocalMap.end()) {
        return;
    }
    auto & LocalRecord = Iter->second;
    ++CheckAndSetLocalAuthId(LocalRecord)->ReferenceCount;

    DEBUG_LOG("FoundAuthRecord:%s", LocalRecord.ToString().c_str());

    auto & Result              = Future.Result;
    Result                     = {};
    Result->LocalAuthId        = LocalRecord.LocalAuthId;
    Result->GlobalAuthId       = LocalRecord.GlobalAuthId;
    Result->ProxyAccessAddress = LocalRecord.ProxyClientAddress;
    Result->ExportAddress      = LocalRecord.StaticExportAddress;
    Result->EnableTcp          = LocalRecord.EnableTcp;
    Result->EnableUdp          = LocalRecord.EnableUdp;
    return;
}

void xAuthLocalService::ReleaseAuthInfo(uint64_t LocalAuthId, const xLocalUsageAudit & Usage) {
    assert(LocalAuthId);
    assert(LocalAuditNodePool.CheckAndGet(LocalAuthId)->LocalAuthId == LocalAuthId);

    auto & LocalAuditNode       = LocalAuditNodePool[LocalAuthId];
    //
    auto & Audit                = LocalAuditNode.Audit;
    Audit.LocalTcpUploadSize   += Usage.LocalTcpUploadSize;
    Audit.LocalTcpDownloadSize += Usage.LocalTcpDownloadSize;
    Audit.LocalUdpUploadSize   += Usage.LocalUdpUploadSize;
    Audit.LocalUdpDownloadSize += Usage.LocalUdpDownloadSize;

    --LocalAuditNode.ReferenceCount;
}

xAuthLocalUsageAuditNode * xAuthLocalService::CheckAndSetLocalAuthId(xAuthLocalRecord & LocalRecord) {
    if (LocalRecord.LocalAuthId) {
        assert(LocalAuditNodePool.CheckAndGet(LocalRecord.LocalAuthId));
        return &LocalAuditNodePool[LocalRecord.LocalAuthId];
    }
    auto LocalAuthId = LocalAuditNodePool.Acquire();
    X_RUNTIME_ASSERT(LocalRecord.LocalAuthId = LocalAuthId);

    auto & LocalAuditNode      = LocalAuditNodePool[LocalAuthId];
    LocalAuditNode.LocalAuthId = LocalAuthId;
    LocalAuditNode.TimestampMS = LocalAuditNode.Audit.CollectionStartTimestampMS = LocalTicker();
    LocalAuditTimeoutList.AddTail(LocalAuditNode);
    ++Audit.UnReleasedAuthInfoCount;

    return &LocalAuditNode;
}

static void LoadFile(xAuthLocalMap & TargetMap, const fs::path & FilePath) {
    auto   Doc      = rapidcsv::Document(FilePath.string(), rapidcsv::LabelParams(0, 0));
    size_t RowCount = Doc.GetRowCount();  // 数据行数（不含表头）

    auto AuthIdIndex          = Doc.GetColumnIdx("auth_id");
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
        auto AuthId          = (uint64_t)strtoumax(Doc.GetCell<std::string>(AuthIdIndex, Row).c_str(), nullptr, 10);
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

        Node.GlobalAuthId        = AuthId;
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
