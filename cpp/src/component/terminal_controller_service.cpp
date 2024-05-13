#include "./terminal_controller_service.hpp"

static constexpr const size_t   DEFAULT_MAX_CONNECTION_PAIR_SIZE   = 20'0000;
static constexpr const uint64_t DEFAULT_CONNECTION_PAIR_TIMEOUT_MS = 10 * 60'000;

bool xTerminalController::Init(xIoContext * IoCtxPtr, const xNetAddress & BindAddress, size_t MaxProxyConnection) {
	RuntimeAssert(xService::Init(IoCtxPtr, BindAddress, MaxProxyConnection, true));
	RuntimeAssert(ConnectionPairPool.Init(DEFAULT_MAX_CONNECTION_PAIR_SIZE));
	RuntimeAssert(ConnectionPairTimeoutList.IsEmpty());
	return true;
}

void xTerminalController::Clean() {
	ConnectionPairPool.Clean();
	xService::Clean();
}

void xTerminalController::Tick(uint64_t NowMS) {
	Ticker.Update(NowMS);
	xService::Tick(Ticker);
	ClearTimeoutRelayConnectionPairs();
	OutputAudit();
}

xRelayConnectionPair * xTerminalController::CreateConnectionPair() {
	auto PairId = ConnectionPairPool.Acquire();
	if (!PairId) {
		return nullptr;
	}
	auto PP              = &ConnectionPairPool[PairId];
	PP->ConnectionPairId = PairId;
	KeepAlive(PP);
	++Audit_ConnectionPairCount;
	return PP;
}

xRelayConnectionPair * xTerminalController::GetConnectionPairById(uint64_t ConnectionPairId) {
	return ConnectionPairPool.CheckAndGet(ConnectionPairId);
}

void xTerminalController::DestroyConnectionPair(uint64_t ConnectionPairId) {
	auto PP = ConnectionPairPool.CheckAndGet(ConnectionPairId);
	if (!PP) {
		return;
	}
	DestroyConnectionPair(PP);
}

void xTerminalController::DestroyConnectionPair(xRelayConnectionPair * PairPtr) {
	ConnectionPairPool.Release(PairPtr->ConnectionPairId);
	--Audit_ConnectionPairCount;
}

void xTerminalController::KeepAlive(xRelayConnectionPair * PairPtr) {
	PairPtr->TimestampMS = Ticker;
	ConnectionPairTimeoutList.GrabTail(*PairPtr);
}

void xTerminalController::ClearTimeoutRelayConnectionPairs() {
	auto C = [KillTimepoint = Ticker() - DEFAULT_CONNECTION_PAIR_TIMEOUT_MS](const xRelayConnectionPair & N) { return N.TimestampMS < KillTimepoint; };
	while (auto NP = ConnectionPairTimeoutList.PopHead(C)) {
		OnDestroyTimeoutConnectionPair(NP);
		DestroyConnectionPair(NP);
	}
}

void xTerminalController::OutputAudit() {
	if (Ticker() - Audit_TimestampMS < 60'000) {
		return;
	}
	Audit_TimestampMS = Ticker();
	cout << "xTerminalController Audit_ConnectionPairCount=" << Audit_ConnectionPairCount << endl;
}
