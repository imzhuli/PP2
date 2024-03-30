#include "./terminal_service.hpp"

bool xTerminalService::Init(xIoContext * IoCtxPtr, size_t ConnectionPoolSize) {
	if (!TerminalConnectionPool.Init(ConnectionPoolSize)) {
		return false;
	}
	this->IoCtxPtr = IoCtxPtr;
	this->Ticker.Update();

	return true;
}

void xTerminalService::Clean() {
	KillConnectionList.GrabListTail(IdleConnectionList);
	KillConnections();
	TerminalConnectionPool.Clean();
}

void xTerminalService::Tick(uint64_t NowMS) {
	Ticker.Update(NowMS);

	// TODO: IdleConnectionList

	//
	KillConnections();
}

uint64_t xTerminalService::CreateTerminalConnection(const xTerminalConnectionOptions & Options) {
	auto Id = TerminalConnectionPool.Acquire();
	auto CP = &TerminalConnectionPool[Id];
	if (!CP->Init(IoCtxPtr, Options.TargetAddress, Options.BindAddress, this)) {
		TerminalConnectionPool.Release(Id);
		return {};
	}
	CP->ConnectionId    = Id;
	CP->LastTimestampMS = Ticker;
	return Id;
}

void xTerminalService::DestroyTerminalConnection(uint64_t ConnectionId) {
	auto CP = TerminalConnectionPool.CheckAndGet(ConnectionId);
	if (!CP) {
		return;
	}
	KillConnectionList.GrabTail(*CP);
}

void xTerminalService::KillConnections() {
	for (auto & N : KillConnectionList) {
		auto CP = UpCast(&N);
		CP->Clean();
		TerminalConnectionPool.Release(CP->ConnectionId);
	}
}
