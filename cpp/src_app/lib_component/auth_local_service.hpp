#pragma once
#include "./abstract/auth_abstract.hpp"

#include <filesystem>
#include <pp_common/_common.hpp>

struct xAuthLocalRecord final {
    xCountryId CountryId = 0;
    bool       EnableUdp = false;
};
using xAuthLocalMap = std::unordered_map<std::string, xAuthLocalRecord>;

class xAuthLocalService final : public xAuthAbstractService {
public:
    bool   Init(const char * auth_dir);
    void   Clean();
    size_t GetUnprocessedResultCount() const override { return Audit.ResultCount; }

    void Validate(const std::string_view AccountPass, xAuthResultFuture & Future) override;
    void ReleaseAuthResult(xAuthResult * Result) override;

private:
    void ReloadAuthFile();

private:
    std::filesystem::path AuthFileDir;
    std::thread           ReloadAuthFileThread;
    xel::xRunState        RunState;

    xel::xMemoryPool<xAuthResult> ResultPool;
    xAuthLocalMap                 AuthLocalMap;

    struct {
        size_t ResultCount = 0;
    } Audit;
};
