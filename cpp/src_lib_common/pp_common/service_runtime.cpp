#include "./service_runtime.hpp"

#include <fstream>
#include <mutex>

/// @brief Common code in ////////

static xIoContext      ServiceIoContextInstance;
static xel::xStdLogger StdLogger;
static xel::xNonLogger NonLogger;

xRuntimeEnv  RuntimeEnv                 = {};
xLogger *    Logger                     = &NonLogger;
xLogger *    AuditLogger                = &NonLogger;
xLogger *    ConsoleLogger              = &StdLogger;
xIoContext * ServiceIoContext           = nullptr;
uint64_t     ServiceIoLoopOnceTimeoutMS = 10;
xTicker      ServiceTicker              = {};
xRunState    ServiceRunState            = {};

static void InitLogger() {
    Logger = new xBaseLogger();
    RuntimeAssert(static_cast<xBaseLogger *>(Logger)->Init(std::string(RuntimeEnv.DefaultLoggerFilePath).c_str()));
    Logger->SetLogLevel(eLogLevel::Debug);

    AuditLogger = new xBaseLogger();
    RuntimeAssert(static_cast<xBaseLogger *>(AuditLogger)->Init(std::string(RuntimeEnv.DefaultAuditLoggerFilePath).c_str()));
}

static void CleanLogger() {
    RuntimeAssert(AuditLogger);
    static_cast<xBaseLogger *>(AuditLogger)->Clean();
    delete Steal(AuditLogger, &NonLogger);

    RuntimeAssert(Logger);
    static_cast<xBaseLogger *>(Logger)->Clean();
    delete Steal(Logger, &NonLogger);
}

static auto Instance = (xServiceRuntimeEnvGuard *){};
static auto EnvMutex = std::mutex();

xServiceRuntimeEnvGuard::xServiceRuntimeEnvGuard(int argc, char ** argv, bool EnableDefaultLogger) : EnableLogger(EnableDefaultLogger) {
    auto G = std::lock_guard(EnvMutex);
    RuntimeAssert(!Instance);

    RuntimeEnv = xRuntimeEnv::FromCommandLine(argc, argv);
    if (EnableLogger) {
        InitLogger();
    }
    ServiceIoContext = &ServiceIoContextInstance;
    RuntimeAssert(ServiceIoContext->Init());
    RuntimeAssert(ServiceRunState.Start());
    Instance = this;
}

xServiceRuntimeEnvGuard::~xServiceRuntimeEnvGuard() {
    auto G = std::lock_guard(EnvMutex);
    RuntimeAssert(this == Instance);

    ServiceRunState.Finish();
    Steal(ServiceIoContext)->Clean();
    if (EnableLogger) {
        CleanLogger();
    }
    Reset(RuntimeEnv);
    Reset(Instance);
}

/////////////////////////

bool xServiceRequestContextPool::Init(size_t PoolSize) {
    return Pool.Init(PoolSize);
}

void xServiceRequestContextPool::Clean() {
    Pool.Clean();
    assert(TimeoutList.IsEmpty());
}

auto xServiceRequestContextPool::Acquire(xVariable RequestContext, xVariable RequestContextEx) -> const xServiceRequestContext * {
    auto Id = Pool.Acquire();
    if (!Id) {
        return nullptr;
    }
    auto & N             = Pool[Id];
    N.RequestId          = Id;
    N.RequestTimestampMS = ServiceTicker();
    TimeoutList.AddTail(N);

    N.RequestContext   = RequestContext;
    N.RequestContextEx = RequestContextEx;

    return &N;
}

auto xServiceRequestContextPool::CheckAndGet(uint64_t RequestId) -> const xServiceRequestContext * {
    return Pool.CheckAndGet(RequestId);
}

void xServiceRequestContextPool::Release(const xServiceRequestContext * RCP) {
    assert(Pool.CheckAndGet(RCP->RequestId) == RCP);
    Pool.Release(RCP->RequestId);
}
