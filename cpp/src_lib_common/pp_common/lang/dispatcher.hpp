#pragma once
#include "../_.hpp"

template <typename tEvent, size_t MaxFunctionSize = 16>
class xDispatcher : xel::xNonCopyable {
private:
    using xEvent    = std::remove_cvref_t<tEvent>;
    using xFunction = std::function<void(const xEvent &)>;
    struct xFunctionNode : xListNode {
        xFunction Function;
        xFunctionNode(const xFunction & F) : Function(F) {}
    };

public:
    uint64_t Append(const xFunction & F) {
        if (auto Index = FunctionPool.Acquire(F)) {
            FunctionList.AddTail(FunctionPool[Index]);
            return Index;
        }
        return 0;
    }
    void Remove(uint64_t Key) { FunctionPool.CheckAndRelease(Key); }

    uint64_t operator+=(const xFunction & F) { return Append(F); }
    void     operator-=(uint64_t Index) { Remove(Index); }

    void operator()(const tEvent & Event) const {
        FunctionList.ForEach([&](const xFunctionNode & FN) { FN.Function(Event); });
    }

    xFunction Function() const { return Delegate(&xDispatcher::operator(), this); }

private:
    xList<xFunctionNode>                                       FunctionList;
    xel::xIndexedStorageStatic<xFunctionNode, MaxFunctionSize> FunctionPool;
};
