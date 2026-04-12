#pragma once
#include "./auth_abstract.hpp"

#include <filesystem>
#include <pp_common/_common.hpp>

class xAuthLocalService final : public xAuthAbstract {
public:
    bool Init(const char * auth_dir);
    void Clean();

    bool Validate(const std::string_view Account, const std::string_view Pass, xPA_AuthFeature & Future) override;

private:
    void ReloadAuthFile();

private:
    std::filesystem::path AuthFileDir;
    std::thread           ReloadAuthFileThread;
    xel::xRunState        RunState;
};
