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
    bool        Init(const char * auth_dir);
    void        Clean();
    std::string OutputAudit() const;

    void Validate(const std::string_view AccountPass, xAuthResultFuture & Future) override;

private:
    void ReloadAuthFile();

private:
    std::filesystem::path AuthFileDir;
    std::thread           ReloadAuthFileThread;
    xel::xRunState        RunState;
    xAuthLocalMap         AuthLocalMap;

    struct {
        size_t CachedAuthInfoCount = 0;
    } Audit;
};
