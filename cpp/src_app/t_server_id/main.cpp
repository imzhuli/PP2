#include "../lib_component/server_id_manager.hpp"

static auto M = xServerIdManager();

static uint64_t IDA[xObjectIdManager::MaxObjectId * 2] = {};

int main(int, char **) {
    X_VAR xResourceGuard(M);

    for (auto & R : IDA) {
        R = M.AcquireServerId(0xFF);
    }

    size_t Counter = 0;
    for (auto & R : IDA) {
        if (!R) {
            continue;
        }
        RuntimeAssert(0xFF == xServerIdManager::ExtractServerType(R));
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
