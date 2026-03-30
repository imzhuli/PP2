#include "./local_message_queue.hpp"

using xLocalMessagePool = xel::xMemoryPool<xLocalMessage>;
using xMessageList      = xList<xMessageNode>;

static xLocalMessagePool LocalMessagePool;
static xel::xSpinlock    LocalMessagePoolLock;
static xMessageList      LocalMessageQueue;
static xel::xSpinlock    LocalMessageQueueLock;

static X_VAR xResourceGuard(LocalMessagePool, xMemoryPoolOptions{ .MaxPoolSize = 20'0000 });

xLocalMessage * AllocLocalMessage() {
    return LocalMessagePool.Create();
}

void ReleaseLocalMessage(xLocalMessage * Message) {
    LocalMessagePool.Destroy(Message);
}

void PushMessage(xLocalMessage * Message) {
    assert(!xListNode::IsLinked(*Message));
    LocalMessageQueue.AddTail(*Message);
}

xLocalMessage * PopMessage() {
    return (xLocalMessage *)LocalMessageQueue.PopHead();
}

//////// multi thread safe functions:

xLocalMessage * MT_AllocLocalMessage() {
    X_VAR xel::xSpinlockGuard(LocalMessagePoolLock);
    return AllocLocalMessage();
}

void MT_ReleaseLocalMessage(xLocalMessage * Message) {
    X_VAR xel::xSpinlockGuard(LocalMessagePoolLock);
    return ReleaseLocalMessage(Message);
}

void MT_PushMessage(xLocalMessage * Message) {
    X_VAR xel::xSpinlockGuard(LocalMessageQueueLock);
    PushMessage(Message);
}

xLocalMessage * MT_PopMessage() {
    X_VAR xel::xSpinlockGuard(LocalMessageQueueLock);
    return PopMessage();
}
