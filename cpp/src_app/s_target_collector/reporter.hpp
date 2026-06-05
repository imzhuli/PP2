#pragma once

#include <pp_common/_.hpp>

struct xKfkContext;

class xTargetCollectReporter {
public:
    bool Init(const std::string & ConfigFilename);
    void Clean();
    void Tick(uint64_t NowMS);

private:
    struct xTargetCollectNode
        : xListNode {
        uint64_t    NodeId;
        uint64_t    GlobalAuthId;
        xNetAddress TargetAddress;
        std::string TargetHost;
        size_t      Count;
    };
    using xTargetCollectList = xList<xTargetCollectNode>;

    void PostTargetCollect(uint64_t GlobalAuthId, const xel::xNetAddress & TargetAddress, const std::string_view & TargetHost, size_t Count);
    void KfkThreadFunc();

private:
    xRunState                       RunState;
    std::thread                     KfkThread;
    //
    xMemoryPool<xTargetCollectNode> NodePool;
    xSpinlock                       SwitchListLock;
    xTargetCollectList              PostCollectionList;
    xTargetCollectList              PendingCollectionList;
    xTargetCollectList              FinishedCollectionList;
    xKfkContext *                   KfkContext = nullptr;
};