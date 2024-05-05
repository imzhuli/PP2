#pragma once
#include "../common/base.hpp"

#include <unordered_map>

struct xAuthQuery : xQueueNode {
	uint64_t SourceRequestId = 0;
};

struct xAuthResult {
	uint64_t    AuditKey;                   // 绑定的计量账号(不是计费)
	xNetAddress TerminalControllerAddress;  // relay server, or terminal service address
	uint64_t    TerminalId;                 // index in relay server
	bool        EnableUdp;
};

struct xAuthCacheNode : xListNode {
	uint64_t           TimestampMS;
	xAuthResult        AuthInfo;
	uint64_t           QueryId;
	xQueue<xAuthQuery> QueryQueue;
};

class xAuthCacheService {
	using xAuthMap = std::unordered_map<std::string, xAuthCacheNode *>;

public:
	bool Init();
	void Clean();
	void Tick(uint64_t NowMS);  // remove timeout nodes

	static std::string MakeKey(const std::string_view AccountView, const std::string_view PasswordView);

private:
	xTicker                 Ticker;
	xMemoryPool<xAuthQuery> QueryNodePool;
	xAuthMap                AuthMap;
};
