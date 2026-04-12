#include "./auth_local.hpp"

static constexpr const auto RELOAD_TIMEOUT = 60s;

bool xAuthLocalService::Init(const char * AuthFileDir) {
    assert(AuthFileDir);
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
}

bool xAuthLocalService::Validate(const std::string_view Account, const std::string_view Pass, xPA_AuthFeature & Future) {

    Future.SetReady();
    return true;
}

void xAuthLocalService::ReloadAuthFile() {
    auto ReloadTimer = xTimer(ZeroInit);
    while (RunState) {
        if (!ReloadTimer.TestAndTag(RELOAD_TIMEOUT)) {
            std::this_thread::sleep_for(1s);
            continue;
        }

        cout << "ReloadFile from: " << AuthFileDir << endl;
    }
}