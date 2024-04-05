#pragma once
#include "../common/base.hpp"
#include "../common_protocol/client_auth.hpp"
#include "../common_protocol/network.hpp"
#include "../common_protocol/terminal.hpp"

#include <unordered_map>

static constexpr const uint64_t TCP_CONNECTION_AUTH_TIMEOUT_MS  = 15'000;  // auth and connection
static constexpr const uint64_t TCP_CONNECTION_IDLE_TIMEOUT_MS  = 125'000;
static constexpr const uint64_t TCP_CONNECTION_FLUSH_TIMEOUT_MS = 3'000;
static constexpr const uint64_t MAX_PROXY_RELAY_CONNECTION      = 10'000;
static constexpr const uint64_t MAX_PROXY_CLIENT_CONNECTION     = 10'000;

class xProxyClientConnection;
class xProxyRelayClient;
class xProxyDispatcherClient;
class xProxyService;

enum eProxyType {
	PROXY_TYPE_UNSPEC = 0,
	PROXY_TYPE_SOCKS5 = 1,
	PROXY_TYPE_HTTP   = 2,
};

enum eClientState {
	CLIENT_STATE_INIT = 0,

	CLIENT_STATE_S5_WAIT_FOR_CLIENT_AUTH,
	CLIENT_STATE_S5_WAIT_FOR_AUTH_RESULT,
	CLIENT_STATE_S5_AUTH_DONE,
	CLIENT_STATE_S5_AUTH_FAILED,

	CLIENT_STATE_S5_WAIT_FOR_DNS_RESULT,
	CLIENT_STATE_S5_TCP_CONNECTING,
	CLIENT_STATE_S5_TCP_ESTABLISHED,
	CLIENT_STATE_S5_TCP_CLOSED,

	CLIENT_STATE_S5_UDP_BINDING,
	CLIENT_STATE_S5_UDP_READY,
	CLIENT_STATE_S5_UDP_EXPIRED,

	CLIENT_STATE_HTTP_WAIT_FOR_AUTH_RESULT,
	CLIENT_STATE_HTTP_AUTH_DONE,
	CLIENT_STATE_HTTP_AUTH_FAILED,

	CLIENT_STATE_HTTP_TCP_QUERY_HOST,
	CLIENT_STATE_HTTP_TCP_CONNECTING,
	CLIENT_STATE_HTTP_TCP_ESTABLISHED,
	CLIENT_STATE_HTTP_TCP_CLOSED,
};

enum eHttpState {};

struct xProxyClientIdleNode : xListNode {
	uint64_t TimestampMS;
};

class xProxyClientConnection
	: public xTcpConnection
	, public xProxyClientIdleNode {
public:
	eProxyType   Type                       = PROXY_TYPE_UNSPEC;
	eClientState State                      = CLIENT_STATE_INIT;
	xIndexId     ClientConnectionId         = {};
	xIndexId     TerminalControllerIndex    = {};
	xNetAddress  TerminalControllerAddress  = {};
	xIndexId     TerminalControllerSubIndex = {};
	xIndexId     TerminalConnectionId       = {};
	xNetAddress  TargetAddress              = {};

	void SetCloseOnFlush() {
		CloseOnFlush = true;
	}
	bool CloseOnFlush = false;
};

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

protected:
	size_t OnData(xTcpConnection * TcpConnectionPtr, void * DataPtrInput, size_t DataSize) override;
	bool   OnPacket(const xPacketHeader & Header, ubyte * PayloadPtr, size_t PayloadSize);

protected:
	xProxyService * ProxyServicePtr = nullptr;
};

class xProxyDispatcherClient : public xClient {
public:
	bool Init(xIoContext * IoContextPtr, const xNetAddress & TargetAddress, xProxyService * ProxyServicePtr) {
		RuntimeAssert(xClient::Init(IoContextPtr, TargetAddress, xNetAddress()));
		this->ProxyServicePtr = ProxyServicePtr;
		return true;
	}
	void Clean() {
		xClient::Clean();
		Reset(ProxyServicePtr);
	}

protected:
	bool OnPacket(const xPacketHeader & Header, ubyte * PayloadPtr, size_t PayloadSize) override;

protected:
	xProxyService * ProxyServicePtr = nullptr;
};

class xProxyService
	: public xTcpConnection::iListener
	, public xTcpServer::iListener {
private:
	friend class xProxyRelayClient;
	friend class xProxyDispatcherClient;

public:
	bool Init(xIoContext * IoCtxPtr, const xNetAddress & BindAddress, const xNetAddress & DispatcherAddress);
	void Clean();
	void Tick(uint64_t NowMS);

	size_t GetAuditClientConnectionCount() const {
		return AuditClientConnectionCount;
	}

protected:
	xProxyClientConnection * UpCast(xTcpConnection * TcpConnectionPtr) {
		return static_cast<xProxyClientConnection *>(TcpConnectionPtr);
	}
	size_t OnData(xTcpConnection * TcpConnectionPtr, void * DataPtr, size_t DataSize) override;
	void   OnFlush(xTcpConnection * TcpConnectionPtr) override {
        auto CCP = UpCast(TcpConnectionPtr);
        X_DEBUG_PRINTF("OnFlushKill, CID=%" PRIx64 "", CCP->ClientConnectionId);
        if (CCP->CloseOnFlush) {
            KillClientConnection(CCP);
        }
	}
	void OnPeerClose(xTcpConnection * TcpConnectionPtr) override {
		KillClientConnection(UpCast(TcpConnectionPtr));
	}

	// from dispatch server
	void OnAuthResponse(uint64_t ClientConnectionId, const xProxyClientAuthResp & AuthResp);
	void OnDnsResponse(uint64_t ClientConnectionId, const xHostQueryResp & DnsResp);
	void OnTerminalConnectionResult(const xCreateTerminalConnectionResp & Result);

protected:
	void   OnNewConnection(xTcpServer * TcpServerPtr, xSocket && NativeHandle) override;
	size_t OnClientInit(xProxyClientConnection * CCP, void * DataPtr, size_t DataSize);
	size_t OnClientS5Auth(xProxyClientConnection * CCP, void * DataPtr, size_t DataSize);
	size_t OnClientS5Connect(xProxyClientConnection * CCP, void * DataPtr, size_t DataSize);

	void                PostAuthRequest(xProxyClientConnection * CCP, const std::string_view AccountNameView, const std::string_view PasswordView);
	void                PostDnsRequest(xProxyClientConnection * CCP, const std::string & Hostname);
	xProxyRelayClient * MakeS5NewConnection(xProxyClientConnection * CCP);
	void                KeepAlive(xProxyRelayClient * RCP) {
        RCP->KeepAliveTimestampMS = NowMS;
        RelayClientKeepAliveList.GrabTail(*RCP);
	}
	void CreateTargetConnection(xProxyRelayClient * RCP, xIndexId ClientConnectionId, const xNetAddress & Target);
	void DestroyTargetConnection(xIndexId SourceConnectionId);

	void FlushAndKillClientConnection(xProxyClientIdleNode * NodePtr) {
		auto CCP = static_cast<xProxyClientConnection *>(NodePtr);
		if (CCP->HasPendingWrites()) {
			CCP->SetCloseOnFlush();
			ClientFlushTimeoutList.GrabTail(*CCP);
			return;
		}
		ClientKillList.GrabTail(*NodePtr);
	}
	void KillClientConnection(xProxyClientIdleNode * NodePtr) {
		ClientKillList.GrabTail(*NodePtr);
	}
	void ShrinkAuthTimeout();
	void ShrinkIdleTimeout();
	void ShrinkFlushTimeout();
	void ShrinkKillList();
	void ShrinkRelayClient();

protected:
	xIoContext *                            IoCtxPtr = nullptr;
	uint64_t                                NowMS;
	xTcpServer                              TcpServer;
	xProxyDispatcherClient                  DispatcherClient;
	xIndexedStorage<xProxyRelayClient>      RelayClientPool;
	xIndexedStorage<xProxyClientConnection> ClientConnectionPool;

	xList<xProxyClientIdleNode> ClientAuthTimeoutList;
	xList<xProxyClientIdleNode> ClientIdleTimeoutList;
	xList<xProxyClientIdleNode> ClientFlushTimeoutList;
	xList<xProxyClientIdleNode> ClientKillList;

	std::unordered_map<std::string, uint64_t> RelayClientMap;
	xList<xProxyRelayClientNode>              RelayClientKeepAliveList;

	size_t AuditClientConnectionCount = 0;
};
