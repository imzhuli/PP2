#pragma once
#include <config/config.hpp>
#include <core/core_min.hpp>
#include <core/core_os.hpp>
#include <core/core_stream.hpp>
#include <core/core_time.hpp>
#include <core/executable.hpp>
#include <core/functional.hpp>
#include <core/indexed_storage.hpp>
#include <core/list.hpp>
#include <core/logger.hpp>
#include <core/memory.hpp>
#include <core/memory_pool.hpp>
#include <core/optional.hpp>
#include <core/string.hpp>
#include <crypto/base64.hpp>
#include <crypto/md5.hpp>
#include <network/net_address.hpp>
#include <network/tcp_connection.hpp>
#include <network/tcp_server.hpp>
#include <network/udp_channel.hpp>
#include <object/object.hpp>
#include <server_arch/client.hpp>
#include <server_arch/client_pool.hpp>
#include <server_arch/client_wrapper.hpp>
#include <server_arch/message.hpp>
#include <server_arch/tcp_service.hpp>
#include <server_arch/udp_service.hpp>

//
#include <cinttypes>

//
using namespace xel::common;
using namespace std::chrono_literals;

// consts
using xel::InvalidDataSize;
using xel::MaxPacketPayloadSize;
using xel::MaxPacketSize;
using xel::PacketHeaderSize;

// type-defs
using xel::eLogLevel;
using xel::xAbstract;
using xel::xBaseLogger;
using xel::xBinaryMessage;
using xel::xClient;
using xel::xClientPool;
using xel::xClientPoolConnectionHandle;
using xel::xClientWrapper;
using xel::xCommandLine;
using xel::xConfigLoader;
using xel::xIndexedStorage;
using xel::xIndexId;
using xel::xIndexIdPool;
using xel::xIoContext;
using xel::xList;
using xel::xListNode;
using xel::xLogger;
using xel::xMd5Result;
using xel::xMemoryPool;
using xel::xMemoryPoolOptions;
using xel::xNetAddress;
using xel::xObjectIdManager;
using xel::xObjectIdManagerMini;
using xel::xOptional;
using xel::xPacket;
using xel::xPacketCommandId;
using xel::xPacketHeader;
using xel::xPacketRequestId;
using xel::xResourceGuard;
using xel::xScopeCleaner;
using xel::xScopeGuard;
using xel::xSocket;
using xel::xStreamReader;
using xel::xStreamWriter;
using xel::xTcpConnection;
using xel::xTcpServer;
using xel::xTcpService;
using xel::xTcpServiceClientConnectionHandle;
using xel::xTicker;
using xel::xTimer;
using xel::xUdpChannel;
using xel::xUdpService;
using xel::xUdpServiceChannelHandle;

// functions
using xel::Base64Decode;
using xel::Base64Encode;
using xel::BuildPacket;
using xel::Daemonize;
using xel::Delegate;
using xel::FileToLines;
using xel::FileToStr;
using xel::GetTimestampMS;
using xel::HexShow;
using xel::HexToStr;
using xel::JoinStr;
using xel::Md5;
using xel::Noop;
using xel::Pass;
using xel::Pure;
using xel::RuntimeAssert;
using xel::Split;
using xel::Steal;
using xel::StrToHex;
using xel::StrToHexLower;
using xel::Todo;
using xel::Trim;
using xel::Unreachable;
using xel::WriteMessage;
using xel::ZeroFill;

// std-lib:
#include <functional>
#include <iostream>
using std::cerr;
using std::cout;
using std::endl;
using std::flush;
using std::function;

// min_defs:
using xVersion = uint32_t;

using xServerId     = uint64_t;
using xAccountId    = uint64_t;
using xTerminalId   = uint64_t;
using xConnectionId = uint64_t;

struct xServerInfo {
    xServerId   ServerId = {};
    xNetAddress Address  = {};

    std::strong_ordering operator<=>(const xServerInfo &) const = default;
    static bool          LessById(const xServerInfo & LHS, const xServerInfo & RHS) { return LHS.ServerId < RHS.ServerId; }
};

enum struct eRelayServerType : uint16_t {
    UNSPECIFIED = 0,
    DEVICE      = 1,
    THIRD       = 2,
    STATIC      = 3,
};

class xJumpTicker {
public:
    xJumpTicker(uint64_t JumpScaleMS = 1) : JumpScaleMS(JumpScaleMS) {}

    inline void Tick(uint64_t NowMS) {
        if (NowMS - JumpScaleMS < LastTickTimestampMS) {
            return;
        }
        LastTickTimestampMS = NowMS;
        OnTick(NowMS);
    }

    std::function<void(uint64_t)> OnTick = Noop<>;

private:
    const uint64_t JumpScaleMS         = 0;
    uint64_t       LastTickTimestampMS = 0;
};

namespace __pp_common_detail__ {

    template <typename T>
    inline std::enable_if_t<std::is_function_v<T>> __TickOne__(uint64_t NowMS, T * F) {
        (*F)(NowMS);
    }

    template <typename T>
    inline std::enable_if_t<std::is_function_v<T>> __TickOne__(uint64_t NowMS, T & F) {
        F(NowMS);
    }

    template <typename T>
    inline std::enable_if_t<std::is_object_v<T>> __TickOne__(uint64_t NowMS, T & FO) {
        FO.Tick(NowMS);
    }

    inline void __TickAll__(uint64_t) {  // iteration finishes here
    }
    template <typename T, typename... TOthers>
    inline void __TickAll__(uint64_t NowMS, T && First, TOthers &&... Others) {
        __TickOne__(NowMS, std::forward<T>(First));
        __TickAll__(NowMS, std::forward<TOthers>(Others)...);
    }

}  // namespace __pp_common_detail__

template <typename... T>
inline void TickAll(uint64_t NowMS, T &&... All) {
    __pp_common_detail__::__TickAll__(NowMS, std::forward<T>(All)...);
}

// clang-format off

static inline uint32_t High32(uint64_t U) { return (uint32_t)(U >> 32); }
static inline uint32_t Low32(uint64_t U)  { return (uint32_t)(U); }
static inline uint64_t Make64(uint32_t H32, uint32_t L32) { return (static_cast<uint64_t>(H32) << 32) + L32; }

static inline uint16_t High16(uint64_t U) { return (uint16_t)(U >> 48); }
static inline uint64_t Low48(uint64_t U)  { return U & 0x0000'FFFF'FFFF'FFFFu; }
static inline uint64_t Make64_H16L48(uint16_t H16, uint64_t L48) { return (static_cast<uint64_t>(H16) << 48) + L48; }

// clang-format on

// clang-format off

static inline void Shutdown() { QuickExit(); }

template<typename T>
std::unique_ptr<T> P2U(T * && Ptr) { return std::unique_ptr<T>(std::move(Ptr)); }

extern uint32_t HashString(const char * S);
extern uint32_t HashString(const char * S, size_t Len);
extern uint32_t HashString(const std::string_view & S);

extern std::string AppSign(uint64_t Timestamp, const std::string & SecretKey, const void * DataPtr, size_t Size);
static inline std::string AppSign(uint64_t Timestamp, const std::string & SecretKey, const std::string_view& V) { return AppSign(Timestamp, SecretKey, V.data(), V.size()); }

extern bool ValidateAppSign(const std::string & Sign, const std::string & SecretKey, const void * DataPtr, size_t Size);
static inline bool ValidateAppSign(const std::string & Sign, const std::string & SecretKey, const std::string_view& V) { return ValidateAppSign(Sign, SecretKey, V.data(), V.size()); }

// clang-format on
inline std::ostream & operator<<(std::ostream & OS, const xNetAddress & Address) {
    OS << Address.ToString();
    return OS;
}

#define X_AT_EXIT(exit)      auto X_CONCAT_FORCE_EXPAND(__X_AtExit__, __LINE__) = ::xel::xScopeGuard(exit);
#define X_SCOPE(entry, exit) auto X_CONCAT_FORCE_EXPAND(__X_Scope__, __LINE__) = ::xel::xScopeGuard(entry, exit);

#ifndef NDEBUG
#define X_MAYBE_UNUSED [[maybe_unused]]
#else
#define X_MAYBE_UNUSED
#endif
