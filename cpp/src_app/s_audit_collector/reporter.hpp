#pragma once

#include "../lib_component/abstract/audit_abstract.hpp"

#include <pp_common/_.hpp>

struct xKfkContext;

class xAuditCollectReporter {
public:
    bool Init(const std::string & ConfigFilename);
    void Clean();
    void Tick(uint64_t NowMS);
    void PostAuditCollect(const xAuditUsage & UsageInfo);
    void PostBlockAccount(const xAuditBlockAccount & BlockAccountInfo);

    std::string GetAuditOutput() const;

private:
    struct xAuditCollectNode
        : xListNode {
        xPacketCommandId Command;
        union {
            xAuditUsage        UsageInfo;
            xAuditBlockAccount BlockAccountInfo;
        };

        std::string ToString() const;
    };
    using xAuditCollectList = xList<xAuditCollectNode>;

    void KfkThreadFunc();

private:
    xRunState                      RunState;
    std::thread                    KfkThread;
    //
    xMemoryPool<xAuditCollectNode> NodePool;
    xSpinlock                      SwitchListLock;
    xAuditCollectList              PostCollectionList;
    xAuditCollectList              PendingCollectionList;
    xAuditCollectList              FinishedCollectionList;
    xKfkContext *                  KfkContext = nullptr;
};