#pragma once
#include "../common/base.hpp"

static constexpr size_t DEFAULT_TERMINAL_CONNECTION_POOL_SIZE = 10'0000;

struct xTerminalConnectionOptions {
	xNetAddress TargetAddress;
	xNetAddress BindAddress;
};

class xTerminalConnectionNode : public xListNode {};

class xTerminalConnection
	: public xTcpConnection
	, public xTerminalConnectionNode {
public:
	uint64_t ConnectionId;
	uint64_t LastTimestampMS;
};

class xTerminalService : private xTcpConnection::iListener {
public:
	bool Init(xIoContext * IoCtxPtr, size_t ConnectionPoolSize = DEFAULT_TERMINAL_CONNECTION_POOL_SIZE);
	void Clean();
	void Tick(uint64_t NowMS);

public:
	uint64_t CreateTerminalConnection(const xTerminalConnectionOptions & Options);
	void     PostConnectionData(uint64_t ConnectionId, const void * DataPtr, size_t DataSize);
	void     DestroyTerminalConnection(uint64_t ConnectionId);

	xTerminalConnection * GetConnectionById(uint64_t ConnectionId) {
		return TerminalConnectionPool.CheckAndGet(ConnectionId);
	}

protected:
	virtual void OnConnectionEstablished(uint64_t ConnectionId){};
	virtual void OnConnectionClosed(uint64_t ConnectionId){};
	virtual bool OnConnectionData(uint64_t ConnectionId, void * DataPtr, size_t DataSize) {
		return true;
	};

	X_INLINE xTerminalConnection * UpCast(xTcpConnection * TcpConnectionPtr) {
		return static_cast<xTerminalConnection *>(TcpConnectionPtr);
	}
	X_INLINE xTerminalConnection * UpCast(xTerminalConnectionNode * NodePtr) {
		return static_cast<xTerminalConnection *>(NodePtr);
	}

private:  // callbacks
	void OnConnected(xTcpConnection * TcpConnectionPtr) override {
		X_DEBUG_PRINTF("");
		auto CP = UpCast(TcpConnectionPtr);
		OnConnectionEstablished(CP->ConnectionId);
	}
	size_t OnData(xTcpConnection * TcpConnectionPtr, void * DataPtr, size_t DataSize) override {
		X_DEBUG_PRINTF("");
		auto CP = UpCast(TcpConnectionPtr);
		if (OnConnectionData(CP->ConnectionId, DataPtr, DataSize)) {
			return DataSize;
		}
		return InvalidDataSize;
	}
	void OnPeerClose(xTcpConnection * TcpConnectionPtr) override {
		X_DEBUG_PRINTF("");
		auto CP = UpCast(TcpConnectionPtr);
		OnConnectionClosed(CP->ConnectionId);
		KillConnection(CP);
	}

private:
	void KeepAlive(xTerminalConnection * CP) {
		CP->LastTimestampMS = Ticker;
		IdleConnectionList.GrabTail(*CP);
	}
	void KillConnection(xTerminalConnection * CP) {
		KillConnectionList.GrabTail(*CP);
	}
	void KillConnections();

private:
	xIoContext *                         IoCtxPtr = nullptr;
	xIndexedStorage<xTerminalConnection> TerminalConnectionPool;
	xTicker                              Ticker;

	xList<xTerminalConnectionNode> IdleConnectionList;
	xList<xTerminalConnectionNode> KillConnectionList;
};
