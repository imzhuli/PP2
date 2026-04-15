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
    bool             IsReady  = {};

    bool SetReady() {
        assert(!IsReady && !xListNode::IsLinked(*this));
        Manager->GetReadyFutureList().AddTail(*this);
        return IsReady = true;
    }
};

struct xFutureHandle final {
public:
    xFutureHandle() = default;
    xFutureHandle(xFutureBase & FutureRef)
        : Manager(FutureRef.Manager), FutureId(FutureRef.FutureId) {
        assert(Manager && FutureId);
    }
    xFutureHandle(xFutureManager * Manager, uint64_t FutureId)
        : Manager(Manager), FutureId(FutureId) {
        assert(Manager && FutureId);
    }
    operator bool() const { return Manager && FutureId; }

    template <typename T = xFutureHandle>
    T * Get() const {
        assert(bool(*this));
        return static_cast<T *>(Manager->GetFuture(FutureId));
    }

private:
    xFutureManager * Manager  = nullptr;
    uint64_t         FutureId = 0;
};
