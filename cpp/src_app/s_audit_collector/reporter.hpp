#pragma once

#include <pp_common/_.hpp>

struct xKfkContext;

class xTargetCollectReporter {
public:
    bool Init(const std::string & ConfigFilename);
    void Clean();

private:
    struct xTargetCollectNode
        : xListNode {
        uint64_t LastReportTimestampMS = {};
    };
    using xTargetCollectList = xList<xTargetCollectNode>;

    void PostTargetCollect(xTargetCollectNode * Node);

private:
    xKfkContext * KfkContext = nullptr;
};