#pragma once
#include "./auth_abstract.hpp"

#include <filesystem>
#include <pp_common/_common.hpp>

struct xAuthLocalRecord final {
    xCountryId CountryId = 0;
    bool       EnableUdp = false;
};
using xAuthLocalMap = std::unordered_map<std::string, xAuthLocalRecord>;

class xAuthLocalService final : public xAuthAbstractService {
public:
    bool Init(const char * auth_dir);
    void Clean();
    void Tick(uint64_t NowMS);

    void Validate(const std::string_view Account, const std::string_view Pass, xPA_AuthFuture & Future) override;
    void ReleaseAuthResult(uint64_t ResultId) override;

private:
    void ReloadAuthFile();

private:
    std::filesystem::path AuthFileDir;
    std::thread           ReloadAuthFileThread;
    xel::xRunState        RunState;

    xel::xIndexedStorage<xAuthResult> ResultPool;
    xAuthLocalMap                     AuthLocalMap;
};
