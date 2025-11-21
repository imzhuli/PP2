#include "./mysql_core.hpp"

#include <mysql/mysql.h>

#include <mutex>

// constexpr
static constexpr const int64_t IDLE_TIMEOUT_MS = 5 * 60'000;

// global
static auto   InitMutex   = std::mutex();
static size_t InitCounter = 0;

#ifndef ER_CONNECTION_KILLED
#define ER_CONNECTION_KILLED 1927
#endif

bool IsMySqlServerLostError(void * MySql_) {
    auto MySql     = (MYSQL *)MySql_;
    auto ErrorCode = mysql_errno(MySql);
    if (ErrorCode == CR_SERVER_GONE_ERROR || ErrorCode == CR_SERVER_LOST || ErrorCode == ER_CONNECTION_KILLED) {
        [[maybe_unused]] auto TS = std::ctime(XP(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())));
        X_DEBUG_PRINTF("Checking server-lost error: @%s  code=%u: %s", TS, ErrorCode, mysql_error(MySql));
        return true;
    }
    if (ErrorCode) {
        auto ErrorCodeStr = mysql_error(MySql);
        if (ErrorCodeStr) {
            X_DEBUG_PRINTF("Checking non-server-lost error: %u: %s", ErrorCode, mysql_error(MySql));
        }
    }
    return false;
}

static void GlobalInit() {
    auto G = std::lock_guard(InitMutex);
    if (InitCounter++) {
        return;
    }
    RuntimeAssert(!mysql_library_init(0, nullptr, nullptr));
}

static void GlobalClean() {
    auto G = std::lock_guard(InitMutex);
    if (--InitCounter) {
        return;
    }
    mysql_library_end();
}

bool xMySqlConn::Init(const std::string & user, const std::string & password, const std::string & database, const std::string & host, uint16_t port) {
    GlobalInit();
    auto GG = xScopeGuard(GlobalClean);

    assert(Disconnected);
    assert(!NativeHandle.IsEnabled());

    User     = user;
    Password = password;
    Database = database;
    Host     = host;
    Port     = port;

    Tick();  // first attempt,

    GG.Dismiss();
    return true;
}

void xMySqlConn::Clean() {
    if (NativeHandle.IsEnabled()) {
        Close();
    }
    Renew(User);
    Renew(Password);
    Renew(Database);
    Renew(Host);
    Renew(Port);
    GlobalClean();
}

//////////
void xMySqlConn::Open() {
    assert(Disconnected);
    assert(!NativeHandle.IsEnabled());

    auto M = mysql_init(nullptr);
    if (!M) {
        X_DEBUG_PRINTF("Failed to create new mysql object");
        return;
    }
    if (!mysql_real_connect(M, Host.c_str(), User.c_str(), Password.c_str(), Database.c_str(), Port, nullptr, 0)) {
        X_DEBUG_PRINTF("Failed to connect to mysql server");
        mysql_close(M);
        return;
    }
    X_DEBUG_PRINTF("New connection established");

    NativeHandle.EnableValue(M);
    Disconnected = false;
}

void xMySqlConn::Close() {
    assert(NativeHandle.IsEnabled());
    mysql_close((MYSQL *)NativeHandle.Get());
    NativeHandle.Disable();
    Disconnected = true;
}

//////////
void xMySqlConn::Tick(uint64_t NowMS) {
    this->NowMS = NowMS;
    // Try Keepalive:
    CheckReconnection();
    if (!IsReady()) {
        return;
    }
    if (SignedDiff(NowMS, LastTimestampMS) < IDLE_TIMEOUT_MS) {
        return;
    }

    // X_DEBUG_PRINTF("DO SELECT 1");
    auto Conn = (MYSQL *)NativeHandle.Get();
    do {
        if (mysql_query(Conn, "SELECT 1")) {
            break;
        }
        auto Result = mysql_store_result(Conn);
        if (!Result) {
            break;
        }
        mysql_free_result(Result);
    } while (false);
    if (IsMySqlServerLostError(Conn)) {
        Close();
        return;
    }
    LastTimestampMS = NowMS;
}

bool xMySqlConn::CheckReconnection() {
    if (!NativeHandle.IsEnabled()) {
        Open();
        return IsReady();
    }
    if (Disconnected) {
        Close();
        Open();
        return IsReady();
    }
    return true;
}
