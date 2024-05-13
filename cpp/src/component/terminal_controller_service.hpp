#pragma once
#include "../common/base.hpp"

static constexpr const size_t DEFAULT_MAX_TERMINAL_PROXY_CONNECTION_COUNT = 10'000;

struct xRelayConnectionPair : xListNode {
	uint64_t ConnectionPairId = {};
	uint64_t TimestampMS      = {};

	// the following are not set by CreateConnectionPair
	uint64_t  TerminalConnectionId = {};
	uint64_t  TargetConnectionId   = {};
	uint64_t  ProxyConnectionId    = {};
	uint64_t  ClientConnectionId   = {};
	uint64_t  UserFlags            = {};
	xVariable UserCtx              = {};
};

class xTerminalController : protected xService {
public:
	bool Init(xIoContext * IoCtxPtr, const xNetAddress & BindAddress, size_t MaxProxyConnection = DEFAULT_MAX_TERMINAL_PROXY_CONNECTION_COUNT);
	void Clean();
	void Tick(uint64_t NowMS);

protected:
	xRelayConnectionPair * CreateConnectionPair();
	xRelayConnectionPair * GetConnectionPairById(uint64_t ConnectionPairId);
	void                   DestroyConnectionPair(uint64_t ConnectionPairId);
	void                   DestroyConnectionPair(xRelayConnectionPair * PairPtr);
	void                   KeepAlive(xRelayConnectionPair * PairPtr);
	void                   ClearTimeoutRelayConnectionPairs();

protected:
	virtual void OnDestroyTimeoutConnectionPair(xRelayConnectionPair * CP) {};

protected:
	xTicker                               Ticker;
	xIndexedStorage<xRelayConnectionPair> ConnectionPairPool;
	xList<xRelayConnectionPair>           ConnectionPairTimeoutList;

private:
	void     OutputAudit();
	uint64_t Audit_TimestampMS         = 0;
	size_t   Audit_ConnectionPairCount = 0;
};
