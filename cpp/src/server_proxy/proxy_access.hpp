#pragma once
#include "../common/base.hpp"
#include "../common_protocol/client_auth.hpp"
#include "../common_protocol/data_exchange.hpp"
#include "../common_protocol/network.hpp"
#include "../profiler/base.hpp"
#include "./proxy_dispatcher_client.hpp"
#include "./proxy_relay_client.hpp"

#include <unordered_map>

static constexpr const uint64_t TCP_CONNECTION_AUTH_TIMEOUT_MS  = 10'000;  // auth and connection
static constexpr const uint64_t TCP_CONNECTION_IDLE_TIMEOUT_MS  = 125'000;
static constexpr const uint64_t TCP_CONNECTION_FLUSH_TIMEOUT_MS = 3'000;
static constexpr const uint64_t MAX_PROXY_RELAY_CONNECTION      = 10'000;
static constexpr const uint64_t MAX_PROXY_CLIENT_CONNECTION     = 10'000;
static constexpr const uint64_t MAX_PROXY_UDP_RECEIVER          = 50'000;

class xProxyClientConnection;
class xProxyRelayClient;
class xProxyDispatcherClient;
class xProxyService;

enum eClientState {
	CLIENT_STATE_INIT = 0,

	CLIENT_STATE_S5_WAIT_FOR_CLIENT_AUTH,
	CLIENT_STATE_S5_WAIT_FOR_AUTH_RESULT,
	CLIENT_STATE_S5_AUTH_DONE,

	CLIENT_STATE_S5_WAIT_FOR_DNS_RESULT,
	CLIENT_STATE_S5_TCP_CONNECTING,
	CLIENT_STATE_S5_TCP_ESTABLISHED,
	CLIENT_STATE_S5_TCP_CLOSED,

	CLIENT_STATE_S5_UDP_BINDING,
	CLIENT_STATE_S5_UDP_READY,
	CLIENT_STATE_S5_UDP_CLOSED,

	CLIENT_STATE_HTTP_WAIT_FOR_HEADER,
	CLIENT_STATE_HTTP_WAIT_FOR_AUTH_RESULT,
	CLIENT_STATE_HTTP_AUTH_DONE,

	CLIENT_STATE_HTTP_WAIT_FOR_DNS_RESULT,
	CLIENT_STATE_HTTP_TCP_CONNECTING,
	CLIENT_STATE_HTTP_TCP_ESTABLISHED,
	CLIENT_STATE_HTTP_TCP_CLOSED,

	CLIENT_STATE_CLOSED,
};

enum struct eHttpMode : uint_fast8_t {
	UNSPEC = 0,
	NORMAL,
	RAW,
};

struct xProxyClientIdleNode : xListNode {
	uint64_t KeepAliveTimestampMS;
	bool     CloseFlag = false;

	void SetCloseFlag() {
		CloseFlag = true;
	}
};

class xProxyClientConnection
	: public xTcpConnection
	, public xProxyClientIdleNode {
public:
	eClientState State                     = CLIENT_STATE_INIT;
	xIndexId     ClientConnectionId        = {};
	xNetAddress  TerminalControllerAddress = {};
	xIndexId     TerminalControllerId      = {};
	xIndexId     TerminalId                = {};
	xIndexId     ConnectionPairId          = {};
	xNetAddress  TargetAddress             = {};  // ignored by UdpBinding, udp bind use tcp listening Address.Decay()
	xIndexId     LocalUdpChannelId         = {};
	uint32_t     ClientFlags               = {};
	//
	static constexpr const uint32_t CLIENT_FLAG_ENABLE_UDP = 0x01;

	struct {
		std::string TargetHost      = {};
		std::string AccountName     = {};
		std::string AccountPassword = {};
		std::string Header          = {};
		std::string Unprocessed     = {};
		std::string Body            = {};
		eHttpMode   Mode            = eHttpMode::UNSPEC;
	} Http;

	xConnectionProfiler Profiler;
	void                PostData(const void * DataPtr, size_t DataSize);
};

class xProxyUdpReceiver
	: public xUdpChannel
	, public xListNode {
public:
	uint64_t    KeepAliveTimestampMS = {};
	uint64_t    ClientConnectionId   = {};
	uint64_t    TerminalControllerId = {};
	uint64_t    ConnectionPairId     = {};
	xNetAddress LastSourceAddress    = {};
};

class xProxyService
	: public xTcpConnection::iListener
	, public xTcpServer::iListener
	, public xUdpChannel::iListener {
private:
	friend class xProxyRelayClient;
	friend class xProxyDispatcherClient;

public:
	bool Init(xIoContext * IoCtxPtr, const xNetAddress & BindAddress, const xNetAddress & UdpExportAddress, const xNetAddress & DispatcherAddress);
	void Clean();
	void Tick(uint64_t NowMS);

	size_t GetAuditClientConnectionCount() const {
		return AuditClientConnectionCount;
	}

	void RemoveRelayClient(xProxyRelayClient * PRC) {
		X_DEBUG_PRINTF("ProxyClientRelayClient: Id=%" PRIx64 "", PRC->ConnectionId);
		auto Iter = RelayClientMap.find(PRC->GetTargetAddress().ToString());
		RuntimeAssert(Iter != RelayClientMap.end());
		RelayClientMap.erase(Iter);
		RelayClientKillList.GrabTail(*PRC);
	}

protected:
	xProxyClientConnection * UpCast(xTcpConnection * TcpConnectionPtr) {
		return static_cast<xProxyClientConnection *>(TcpConnectionPtr);
	}
	size_t OnData(xTcpConnection * TcpConnectionPtr, void * DataPtr, size_t DataSize) override;
	void   OnFlush(xTcpConnection * TcpConnectionPtr) override {
        auto CCP = UpCast(TcpConnectionPtr);
        X_DEBUG_PRINTF("OnFlushKill, CID=%" PRIx64 "", CCP->ClientConnectionId);
        if (CCP->CloseFlag) {
            KillClientConnection(CCP);
        }
	}
	void OnPeerClose(xTcpConnection * TcpConnectionPtr) override {
		X_DEBUG_PRINTF("ClientClose");
		KillClientConnection(UpCast(TcpConnectionPtr));
	}
	void OnData(xUdpChannel * ChannelPtr, void * DataPtr, size_t DataSize, const xNetAddress & RemoteAddress) override;

	// from dispatch server
	void OnAuthResponse(uint64_t ClientConnectionId, const xProxyClientAuthResp & AuthResp);
	void OnDnsResponse(uint64_t ClientConnectionId, const xHostQueryResp & DnsResp);
	void OnTerminalConnectionResult(const xCreateRelayConnectionPairResp & Result);
	void OnRelayData(const xRelayToProxyData & Post);
	void OnCloseConnection(const xCloseClientConnection & Post);
	void OnTerminalUdpAssociationResult(const xCreateUdpAssociationResp & Result);
	void OnRelayUdpData(const xRelayToProxyUdpData & Post);

protected:
	void   OnNewConnection(xTcpServer * TcpServerPtr, xSocket && NativeHandle) override;
	size_t OnClientInit(xProxyClientConnection * CCP, void * DataPtr, size_t DataSize);

	size_t OnClientS5Init(xProxyClientConnection * CCP, void * DataPtr, size_t DataSize);
	size_t OnClientS5Auth(xProxyClientConnection * CCP, void * DataPtr, size_t DataSize);
	size_t OnClientS5Connect(xProxyClientConnection * CCP, void * DataPtr, size_t DataSize);
	size_t OnClientS5Data(xProxyClientConnection * CCP, ubyte * DataPtr, size_t DataSize);
	size_t OnClientS5UdpData(xProxyClientConnection * CCP, ubyte * DataPtr, size_t DataSize);
	void   OnS5AuthResult(xProxyClientConnection * CCP, const xProxyClientAuthResp & AuthResp);

	size_t OnClientHttpInit(xProxyClientConnection * CCP, void * DataPtr, size_t DataSize);
	size_t OnClientHttpHeader(xProxyClientConnection * CCP, void * DataPtr, size_t DataSize);
	void   ProcessHttpHeaderLine(xProxyClientConnection * CCP, const std::string & Line);
	bool   OnHttpHeaderDone(xProxyClientConnection * CCP);
	void   OnHttpAuthResult(xProxyClientConnection * CCP, const xProxyClientAuthResp & AuthResp);
	size_t OnClientHttpBody(xProxyClientConnection * CCP, void * DataPtr, size_t DataSize);
	size_t OnClientHttpStream(xProxyClientConnection * CCP, void * DataPtr, size_t DataSize);
	void   TryResolveHttpTargetHost(xProxyClientConnection * CCP);
	void   OnHttpDnsResult(xProxyClientConnection * CCP, const xHostQueryResp & DnsResp);
	void   DoHttpConnect(xProxyClientConnection * CCP);
	void   OnHttpConnected(xProxyClientConnection * CCP, uint64_t NewConnectionPairId);

	xProxyRelayClient * GetTerminalController(xProxyClientConnection * CCP);
	xProxyRelayClient * GetTerminalController(const xNetAddress & AddressKey);
	void                PostDataToTerminalControllerRaw(uint64_t TerminalControllerId, const void * DataPtr, size_t Dataize);
	void                PostDataToTerminalController(xProxyClientConnection * CCP, const void * DataPtr, size_t DataSize);

	void PostAuthRequest(xProxyClientConnection * CCP, const std::string_view AccountNameView, const std::string_view PasswordView);
	void PostDnsRequest(xProxyClientConnection * CCP, const std::string & Hostname);
	void CreateTargetConnection(xProxyRelayClient * RCP, xProxyClientConnection * CCP);
	void DestroyTargetConnection(xIndexId ClientConnectionId);
	void DestroyTargetConnection(xProxyClientConnection * CCP);
	bool CreateLocalUdpChannel(xProxyClientConnection * CCP);
	void RequestUdpBinding(xProxyRelayClient * RCP, xProxyClientConnection * CCP);
	void DestroyUdpBinding(xProxyClientConnection * CCP);

	void UpdateAntiIdle(xProxyRelayClient * RCP) {
		RCP->LastDataTimestampMS = NowMS;
	}
	void UpdateKeepAlive(xProxyRelayClient * RCP) {
		RCP->KeepAliveTimestampMS = NowMS;
		RelayClientKeepAliveList.GrabTail(*RCP);
	}
	void KeepAlive(xProxyClientConnection * CCP) {
		if (CCP->CloseFlag) {  // already in kill list or kill-on-flush list
			return;
		}
		CCP->KeepAliveTimestampMS = NowMS;
		ClientIdleTimeoutList.GrabTail(*CCP);
	}
	void KeepAliveUseUdpTimeout(xProxyClientConnection * CCP) {
		ClientUsingUdpIdleList.GrabTail(*CCP);
	}
	void KeepAlive(xProxyUdpReceiver * URP) {
		URP->KeepAliveTimestampMS = NowMS;
		UdpReceiverIdleList.GrabTail(*URP);
	}
	void FlushAndKillClientConnection(xProxyClientIdleNode * NodePtr) {
		auto CCP = static_cast<xProxyClientConnection *>(NodePtr);
		CCP->SetCloseFlag();
		if (CCP->HasPendingWrites()) {
			ClientFlushTimeoutList.GrabTail(*CCP);
			return;
		}
		ClientKillList.GrabTail(*NodePtr);
	}
	void KillClientConnection(xProxyClientIdleNode * NodePtr) {
		NodePtr->SetCloseFlag();
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
	xNetAddress                             UdpExportAddress;

	xList<xProxyClientIdleNode> ClientAuthTimeoutList;
	xList<xProxyClientIdleNode> ClientIdleTimeoutList;
	xList<xProxyClientIdleNode> ClientUsingUdpIdleList;
	xList<xProxyClientIdleNode> ClientFlushTimeoutList;
	xList<xProxyClientIdleNode> ClientKillList;

	std::unordered_map<std::string, uint64_t> RelayClientMap;
	xList<xProxyRelayClientNode>              RelayClientKeepAliveList;
	xList<xProxyRelayClientNode>              RelayClientKillList;

	xIndexedStorage<xProxyUdpReceiver *> ClientUdpChannelPool;
	xList<xProxyUdpReceiver>             UdpReceiverIdleList;

	size_t AuditClientConnectionCount = 0;
};
