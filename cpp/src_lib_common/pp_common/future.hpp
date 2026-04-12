#pragma once
#include "./_.hpp"

class xFutureManager;
class xFutureBase;
class xFutureHandle;

struct xFutureNode : xListNode {
    uint64_t StartTimestampMS = {};
};
using xFutureList = xList<xFutureNode>;

class xFutureManager : xAbstract {
public:
    virtual auto GetFuture(uint64_t FutureId) -> xFutureBase * = 0;
    virtual auto GetReadyFutureList() -> xFutureList &;
};

struct xFutureBase : xFutureNode {
    xFutureManager * Manager  = {};
    uint64_t         FutureId = {};
    bool             HasValue = {};

    bool SetReady() {
        assert(!HasValue && !xListNode::IsLinked(*this));
        Manager->GetReadyFutureList().AddTail(*this);
        return HasValue = true;
    }
};

struct xFutureHandle final {
    xFutureManager * Manager  = nullptr;
    xFutureBase *    Future   = nullptr;
    uint64_t         FutureId = 0;

    xFutureHandle() = default;
    xFutureHandle(xFutureBase * Future)
        : Manager(Future->Manager), Future(Future), FutureId(Future->FutureId) {
    }
    xFutureHandle(xFutureManager * Manager, xFutureBase * Future, uint64_t FutureId)
        : Manager(Manager), Future(Future), FutureId(FutureId) {
    }

    bool IsValid() {
        assert(Manager && Future && FutureId);
        return Manager->GetFuture(FutureId) == Future;
    }

    xFutureBase * operator->() const { return Future; }
    xFutureBase & operator*() const {
        assert(Future);
        return *Future;
    }
};
