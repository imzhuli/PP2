#pragma once

#include <core/core_min.hpp>
#include <core/core_stream.hpp>
#include <core/core_time.hpp>
#include <core/core_value_util.hpp>
#include <core/executable.hpp>
#include <core/indexed_storage.hpp>
#include <core/memory.hpp>
#include <core/memory_pool.hpp>
#include <core/queue.hpp>
#include <core/string.hpp>
#include <core/thread.hpp>
#include <core/version.hpp>
#include <core/view.hpp>
#include <network/io_context.hpp>
#include <network/net_address.hpp>
#include <network/packet.hpp>
#include <network/tcp_server.hpp>
#include <network/udp_channel.hpp>
#include <server_arch/client.hpp>
#include <server_arch/message.hpp>
#include <server_arch/service.hpp>

using namespace xel;

#include <atomic>
#include <cinttypes>
#include <iostream>
#include <mutex>
#include <string>
#include <tuple>

using std::cerr;
using std::cout;
using std::endl;
using std::flush;
using std::string;

static constexpr const auto NoReturn = std::tie();

// clang-format off
#define CASE_PRINT(x) case x: X_DEBUG_PRINTF("%s", X_STRINGIFY(x)); break

template<typename T>
// pointer to unique_ptr wrapper
std::unique_ptr<T> P2U(T * Ptr) {
    return std::unique_ptr<T>(Ptr);
}

using xNotifierFunc = void(xVariable);
X_STATIC_INLINE void TrivialNotifier(xVariable) {}

std::string DebugSign(const void * DataPtr, size_t Size);
static inline std::string DebugSign(const std::string_view& V) {
    return DebugSign(V.data(), V.size());
}
