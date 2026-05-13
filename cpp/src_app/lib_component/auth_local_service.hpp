#pragma once
#include "./abstract/auth_abstract.hpp"

#include <filesystem>
#include <pp_common/_common.hpp>

struct xStringHash {
    using is_transparent = void;  // 关键标记
    std::size_t operator()(std::string_view sv) const noexcept {
        return std::hash<std::string_view>{}(sv);
    }
};

struct xStringEqual {
    using is_transparent = void;  // 关键标记
    bool operator()(std::string_view a, std::string_view b) const noexcept {
        return a == b;
    }
};

struct xAuthLocalRecord final {
    xCountryId  CountryId = 0;
    bool        EnableTcp = true;
    bool        EnableUdp = false;
    xNetAddress ProxyClientAddress;
    xNetAddress StaticExportAddress;

    std::string ToString() const;
};
using xAuthLocalMap = std::unordered_map<std::string, xAuthLocalRecord, xStringHash, xStringEqual>;

class xAuthLocalService final : public xAuthAbstractService {
public:
    bool        Init(const std::string & AuthFilePath);
    void        Clean();
    std::string OutputAudit() const;

    void Validate(const std::string_view AccountPassView, xAuthResultFuture & Future) override;

private:
    void ReloadAuthFile();

private:
    xel::xRunState        RunState;
    std::filesystem::path AuthFileDir;
    xAuthLocalMap         AuthLocalMap;
    std::thread           ReloadAuthFileThread;

    struct {
        std::vector<std::filesystem::path>           FileList;
        std::vector<std::filesystem::file_time_type> FileTimestampList;
        std::atomic<xAuthLocalMap *>                 AuthMap;
    } LastReloadInfo;

    struct {
        size_t CachedAuthInfoCount      = 0;
        size_t CachedAuthInfoMapVersion = 0;
    } Audit;
};
