#include "./auth.hpp"

#include "./audit.hpp"

#include <unordered_map>

static constexpr const size_t MAX_AUTH_CACHE_SIZE = 20'000;

static auto Ticker       = xTicker();
static auto AuthCache    = xIndexedStorage<xProxyAuthNode>();
static auto AuthCacheMap = std::unordered_map<std::string, xIndexId>();  // name to password

namespace {

	class xAuthRequester : xClient {
	public:
		using xClient::Clean;
		using xClient::Init;
	};
	static xAuthRequester AuthRequester;

}  // namespace

bool InitProxyAuth(xIoContext * IoCtxPtr, const xNetAddress & DispatcherAddress, xNotifier Notifier, xVariable Context) {
	RuntimeAssert(AuthCache.Init(MAX_AUTH_CACHE_SIZE));
	RuntimeAssert(AuthRequester.Init(IoCtxPtr, DispatcherAddress));
	return true;
}

void CleanProxyAuth() {
	AuthCache.Clean();
}

void TickProxyAuth() {
	Ticker.Update();
}

bool PostAuthRequest(xProxyAuthRequestNode & RequestNode, const std::string & User, const std::string & Password) {
	auto [Iter, Inserted] = AuthCacheMap.insert_or_assign(User, 0);
	if (Inserted) {
		auto NewIndex = AuthCache.Acquire();
		if (!NewIndex) {
			++ProxyAudit.AuthCacheOverflow;
			AuthCacheMap.erase(Iter);
			return false;
		}
		// TODO: post request

	} else {
		// append request to auto node & check if notify is done.
	}
	return false;
}
