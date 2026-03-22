#pragma once
#include <pp_common/_.hpp>

class xRemoteWarning {
private:
    struct xWarning {
        size_t   Threshold      = 1;
        uint64_t TimeoutLimitMS = 60'000;
        ///
        size_t   Count               = 0;
        uint64_t LastReportTimestamp = 0;
    };

public:
    bool Init(const xNetAddress & Remote);
    void Clean();
    void Tick(uint64_t NowMS);
    void SetupWarning(size_t Index, size_t Threshold, uint64_t TimeoutMS);
    void Warn(size_t Index);

protected:
    virtual void DoWarn(const xWarning & W);

private:
    static constexpr const size_t MAX_WARNING = 1023;

    xClient  Client;
    xWarning Warnings[MAX_WARNING + 1];
};