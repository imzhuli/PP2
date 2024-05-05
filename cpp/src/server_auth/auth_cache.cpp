#include "./auth_cache.hpp"

static constexpr const size_t DEFAULT_QUERY_NODE_POOL_SIZE = 20'000;

bool xAuthCacheService::Init() {
	auto Options = xMemoryPoolOptions{
		.MaxPoolSize = DEFAULT_QUERY_NODE_POOL_SIZE,
	};
	if (!QueryNodePool.Init(Options)) {
		return false;
	}
	auto PG = MakeResourceCleaner(QueryNodePool);

	Dismiss(PG);
	return true;
}

void xAuthCacheService::Clean() {
	Renew(AuthMap);
	CleanResourceReversed(QueryNodePool);
}

void xAuthCacheService::Tick(uint64_t NowMS) {
	Ticker.Update(NowMS);
	X_PFATAL("unimplemented");
}

std::string xAuthCacheService::MakeKey(const std::string_view AccountView, const std::string_view PasswordView) {
	auto S = std::string(AccountView);
	S.append(1, '\0');
	S.append(PasswordView);
	return S;
}

// const xValue * xAuthCacheService::Get(const xKey & Key) {
// 	auto Iter = Map.find(Key);
// 	if (Iter == Map.end()) {
// 		return nullptr;
// 	}
// 	return GetValue(Iter->second);
// }

// bool xAuthCacheService::Update(const xKey & Key, const xValue & AuthInfo) {
// 	auto NewNode = new (std::nothrow) xValueNode();
// 	if (!NewNode) {
// 		return false;
// 	}
// }
