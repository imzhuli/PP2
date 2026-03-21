#include "./_.hpp"

enum struct xQueueStatus : uint8_t {
    SUCCESS = 0,
    FULL = 1,
    EMPTY = 2,
};

inline bool IsOk(const xQueueStatus & Status) {
    return Status == xQueueStatus::SUCCESS;
}

template <typename T, size_t MAX_SIZE = 5'0000>
class xFixedSizeArrayQueue {
public:
    xFixedSizeArrayQueue() : Head(0), Tail(0), Count(0) {}

    // 入队：队列满时返回FULL，否则SUCCESS
    xQueueStatus Push(const T& msg) {
        if (IsFull()) {
            return xQueueStatus::FULL;
        }

        // 尾指针位置存入数据，尾指针后移（环形取模）
        Data[Tail] = msg;
        Tail = (Tail + 1) % MAX_SIZE;
        Count++;
        return xQueueStatus::SUCCESS;
    }

    // 出队：队列空时返回EMPTY，成功则通过参数传出数据
    xQueueStatus Pop(T& out_msg) {
        if (IsEmpty()) {
            return xQueueStatus::EMPTY;
        }

        // 头指针位置取出数据，头指针后移（环形取模）
        out_msg = Data[Head];
        Head = (Head + 1) % MAX_SIZE;
        Count--;
        return xQueueStatus::SUCCESS;
    }

    // 判断队列是否已满
    bool IsFull() const {
        return Count == MAX_SIZE;
    }

    // 判断队列是否为空
    bool IsEmpty() const {
        return Count == 0;
    }

    // 获取当前队列元素个数
    size_t size() const {
        return Count;
    }

private:
    T Data[MAX_SIZE];  // 底层固定大小数组
    size_t Head;       // 队头指针（出队位置）
    size_t Tail;       // 队尾指针（入队位置）
    size_t Count;      // 当前元素个数（简化满/空判断）
};