#pragma once
#include "../common/base.hpp"
#include "../common_protocol/data_exchange.hpp"
#include "../common_protocol/protocol.hpp"
#include "../component/static_ip_table.hpp"
#include "../component/terminal_controller_service.hpp"

class xTerminalRelay;

struct xRelayUdpChannelNode : xListNode {
	xTerminalRelay * RelayPtr              = {};
	uint64_t         RelayConnectionPairId = {};
	xUdpChannel      UdpChannel            = {};
};

class xRelayUdpChannel final
	: public xUdpChannel::iListener
	, public xRelayUdpChannelNode {
public:
	bool Init(xTerminalRelay * RelayPtr, const xNetAddress & BindAddress, uint64_t RelayConnectionPairId);
	void Clean();

	void OnData(xUdpChannel * ChannelPtr, void * DataPtr, size_t DataSize, const xNetAddress & RemoteAddress) override;
	void OnError(xUdpChannel * ChannelPtr) override {};
};

struct xRelayTerminalConnectionNode : xListNode {
	xTerminalRelay * RelayPtr              = {};
	xTcpConnection   Connection            = {};
	uint64_t         RelayConnectionPairId = {};
	bool             Established           = false;
};

class xRelayTerminalConnection final
	: public xTcpConnection::iListener
	, public xRelayTerminalConnectionNode {
public:
	bool Init(xTerminalRelay * RelayPtr, const xNetAddress & TargetAddress, const xNetAddress & BindAddress, uint64_t RelayConnectionPairId);
	void Clean();

public:
	uint64_t InitTimestampMS X_DEBUG_INIT(GetTimestampMS());

protected:
	void   OnConnected(xTcpConnection * TcpConnectionPtr) override;
	size_t OnData(xTcpConnection * TcpConnectionPtr, void * DataPtr, size_t DataSize) override;
	void   OnPeerClose(xTcpConnection * TcpConnectionPtr) override;
};

class xTerminalRelay : public xTerminalController {
public:
	bool Init(xIoContext * IoCtxPtr, const xNetAddress & BindAddress, const char * TerminalBindingFile);
	void Clean();
	void Tick(uint64_t NowMS);

	xIoContext * GetIoCtxPtr() const {
		return IoCtxPtr;
	}

protected:
	bool        LoadTerminalBindings(const char * TerminalBindingFile);
	xNetAddress GetBindAddress(uint64_t TerminalId);

protected:
	void OnClientConnected(xServiceClientConnection & Connection) override;
	void OnClientClose(xServiceClientConnection & Connection) override;
	bool OnPacket(xServiceClientConnection & Connection, const xPacketHeader & Header, ubyte * PayloadPtr, size_t PayloadSize) override;
	void OnProxyCreateConnection(xServiceClientConnection & Connection, const xPacketHeader & Header, ubyte * PayloadPtr, size_t PayloadSize);
	void OnProxyCloseConnection(xServiceClientConnection & Connection, const xPacketHeader & Header, ubyte * PayloadPtr, size_t PayloadSize);
	void OnProxyToRelay(xServiceClientConnection & Connection, const xPacketHeader & Header, ubyte * PayloadPtr, size_t PayloadSize);
	void OnProxyCreateUdpAssociation(xServiceClientConnection & Connection, const xPacketHeader & Header, ubyte * PayloadPtr, size_t PayloadSize);
	void OnProxyCloseUdpAssociation(xServiceClientConnection & Connection, const xPacketHeader & Header, ubyte * PayloadPtr, size_t PayloadSize);
	void OnProxyToRelayUdp(xServiceClientConnection & Connection, const xPacketHeader & Header, ubyte * PayloadPtr, size_t PayloadSize);

	xRelayTerminalConnection * GetTargetConnection(xRelayConnectionPair * CP);
	xRelayUdpChannel *         GetUdpAssociation(xRelayConnectionPair * RCP);
	void                       SetTargetConnection(xRelayConnectionPair * RCP, xRelayTerminalConnection * RTC);
	void                       SetUdpAssociation(xRelayConnectionPair * RCP, xRelayUdpChannel * RUC);
	void                       KillConnectionPair(xRelayConnectionPair * RCPP);
	void                       KillConnection(xRelayTerminalConnectionNode * NodePtr);
	void                       KillUdpChannel(xRelayUdpChannelNode * NodePtr);

public:
	void OnDestroyTimeoutConnectionPair(xRelayConnectionPair * CP) override;
	void OnTargetConnectionEstablished(xRelayTerminalConnection * CP);
	void OnTargetConnectionData(xRelayTerminalConnection * CP, ubyte * DataPtr, size_t DataSize);
	void OnTargetConnectionClosed(xRelayTerminalConnection * CP);
	void OnTargetUdpData(xRelayUdpChannel * UCP, void * DataPtr, size_t DataSize, const xNetAddress & RemoteAddress);

private:
	xIoContext *                              IoCtxPtr = {};
	std::unordered_map<uint64_t, xNetAddress> BindIpTable;

	xList<xRelayTerminalConnectionNode> KillConnectionList;
	xList<xRelayUdpChannelNode>         KillUdpAssociationList;
};
