#pragma once
#include "../common/base.hpp"

class xRelayConnection : xListNode {
	uint64_t RelayConnectionId;
	uint64_t TerminalId;
	uint64_t TerminalConnectionId;
	uint64_t ProxyId;
	uint64_t ProxyConnectionId;
};

class xProxyConnection : xTcpConnection {};

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

	virtual void OnTerminalConnectionEstablished();
	virtual void OnTerminalConnectionTimeout();  // local timeout
	virtual void OnTerminalConnectionClosed();

private:
	xIndexedStorage<xProxyConnection> ProxyConnectionPool;
};
