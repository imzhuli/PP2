#include "../lib_component/server_id_manager.hpp"

static auto M = xServerIdManager();

static uint64_t IDA[xObjectIdManager::MaxObjectId * 2] = {};

int main(int, char **) {
    X_VAR xResourceGuard(M);

    size_t Counter = 0;
    for (auto & R : IDA) {
        R = M.AcquireServerId((uint8_t)Counter);
        ++Counter;
    }

    Counter = 0;
    for (auto & R : IDA) {
        if (!R) {
            continue;
        }
        cout << std::hex << R << std::dec << endl;
        RuntimeAssert((uint8_t)Counter == xServerIdManager::ExtractServerType(R));
        RuntimeAssert(Counter == xServerIdManager::ExtractServerIndex(R));
        ++Counter;
    }

    for (auto & R : IDA) {
        if (!R) {
            continue;
        }
        xel::RuntimeAssert(M.ReleaseServerId(R));
    }

    for (auto & R : IDA) {
        if (!R) {
            continue;
        }
        xel::RuntimeAssert(R == M.RegainServerId(R));
    }

    return 0;
}
