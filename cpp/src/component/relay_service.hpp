#pragma once
#include "../common/base.hpp"

class xRelayConnection {
	uint64_t ConnectionId;
	uint64_t TerminalId;
	uint64_t TerminalConnectionId;
	uint64_t ProxyId;
	uint64_t ProxyConnectionId;
};

class xProxyConnection : xTcpConnection {
	//
};

class xRelayService : private xService {
public:
	class xTerminalDecryptor {
		uint64_t TerminalId;
	};

public:
	bool Init();
	void Clean();

	void RegisterTerminalInfo();
	void UpdateTerminalInfo();
	void RemoveTerminalInfo();
	void CheckTimeoutTerminalInfo();

	virtual void PostDataToTerminal(uint64_t TerminalId, uint64_t ConnectionId, const void * DataPtr, size_t DataSize);
	virtual void PostDataToProxy(uint64_t SourceTerminalId, uint64_t ConnectionId, const void * DataPtr, size_t DataSize);

private:
	xIndexedStorage<xTerminalDecryptor> TerminalDescryptorPool;
	xIndexedStorage<xRelayConnection>   RelayConnectionPool;
	xIndexedStorage<xProxyConnection>   ProxyConnectionPool;
};
