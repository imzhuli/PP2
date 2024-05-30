#pragma once
#include "../common/base.hpp"
#include "../profiler/base.hpp"

class xProxyService;

struct xProxyRelayClientNode : xListNode {
	xIndexId ConnectionId;
	uint64_t KeepAliveTimestampMS;
	uint64_t LastDataTimestampMS;
};

class xProxyRelayClient
	: public xProxyRelayClientNode
	, public xTcpConnection
	, public xTcpConnection::iListener {
public:
	bool Init(xProxyService * ProxyServicePtr, const xNetAddress & TargetAddress);
	void Clean();

	const xNetAddress & GetTargetAddress() const {
		return TargetAddress;
	}

	void PostData(const void * _, size_t DataSize);

protected:
	void   OnConnected(xTcpConnection * TcpConnectionPtr) override;
	void   OnPeerClose(xTcpConnection * TcpConnectionPtr) override;
	size_t OnData(xTcpConnection * TcpConnectionPtr, void * DataPtrInput, size_t DataSize) override;
	void   OnFlush(xTcpConnection * TcpConnectionPtr) override;
	bool   OnPacket(const xPacketHeader & Header, ubyte * PayloadPtr, size_t PayloadSize);

protected:
	xProxyService *     ProxyServicePtr = nullptr;
	xNetAddress         TargetAddress   = {};
	xConnectionProfiler Profiler;
};
