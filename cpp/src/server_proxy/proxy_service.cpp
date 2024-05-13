#include "../common_protocol/client_auth.hpp"
#include "../common_protocol/data_exchange.hpp"
#include "../common_protocol/network.hpp"
#include "./global.hpp"
#include "./proxy_access.hpp"

// xProxyService
static constexpr const uint64_t MaxRelayKeepAliveTimeout  = 60'000;
static constexpr const uint64_t MaxRelayClientIdleTimeout = 10 * 60'000;

bool xProxyService::Init(xIoContext * IoCtxPtr, const xNetAddress & BindAddress, const xNetAddress & UdpExportAddress, const xNetAddress & DispatcherAddress) {
	assert(IoCtxPtr && BindAddress && DispatcherAddress);
	RuntimeAssert(TcpServer.Init(IoCtxPtr, BindAddress, this));
	RuntimeAssert(DispatcherClient.Init(IoCtxPtr, DispatcherAddress, this));
	RuntimeAssert(RelayClientPool.Init(MAX_PROXY_RELAY_CONNECTION));
	RuntimeAssert(ClientConnectionPool.Init(MAX_PROXY_CLIENT_CONNECTION));
	RuntimeAssert(ClientUdpChannelPool.Init(MAX_PROXY_UDP_RECEIVER));

	this->IoCtxPtr = IoCtxPtr;
	NowMS          = GetTimestampMS();
	Reset(AuditClientConnectionCount);
	this->UdpExportAddress = UdpExportAddress;
	return true;
}

void xProxyService::Clean() {
	Reset(UdpExportAddress);
	Renew(RelayClientMap);
	ClientUdpChannelPool.Clean();
	ClientConnectionPool.Clean();
	RelayClientPool.Clean();
	TcpServer.Clean();
	DispatcherClient.Clean();
}

void xProxyService::Tick(uint64_t NowMS) {
	this->NowMS = NowMS;
	DispatcherClient.Tick(NowMS);

	ShrinkAuthTimeout();
	ShrinkIdleTimeout();
	ShrinkFlushTimeout();
	ShrinkKillList();
	ShrinkRelayClient();
}

void xProxyService::OnNewConnection(xTcpServer * TcpServerPtr, xSocket && NativeHandle) {
	auto NewConnectionId = ClientConnectionPool.Acquire();
	if (!NewConnectionId) {
		XelCloseSocket(NativeHandle);
		return;
	}
	auto NewClientConnectionPtr = &ClientConnectionPool[NewConnectionId];
	if (!NewClientConnectionPtr->Init(IoCtxPtr, std::move(NativeHandle), this)) {
		ClientConnectionPool.Release(NewConnectionId);
		return;
	}
	NewClientConnectionPtr->Profiler.MarkStartConnection();
	X_DEBUG_PRINTF("NewClientConnectionId:%" PRIx64 "", NewConnectionId);
	NewClientConnectionPtr->ClientConnectionId   = NewConnectionId;
	NewClientConnectionPtr->KeepAliveTimestampMS = NowMS;
	ClientAuthTimeoutList.AddTail(*NewClientConnectionPtr);
	++AuditClientConnectionCount;
}

size_t xProxyService::OnData(xTcpConnection * TcpConnectionPtr, void * DataPtr, size_t DataSize) {
	X_DEBUG_PRINTF("Size=%zi", DataSize);
	auto CCP = UpCast(TcpConnectionPtr);

	switch (CCP->State) {
		case CLIENT_STATE_INIT:
			return CCP->Profiler.MarkUpload(OnClientInit(CCP, DataPtr, DataSize));
		case CLIENT_STATE_S5_WAIT_FOR_CLIENT_AUTH:
			return CCP->Profiler.MarkUpload(OnClientS5Auth(CCP, DataPtr, DataSize));
		case CLIENT_STATE_S5_AUTH_DONE:
			return CCP->Profiler.MarkUpload(OnClientS5Connect(CCP, DataPtr, DataSize));
		case CLIENT_STATE_S5_TCP_ESTABLISHED:
			return CCP->Profiler.MarkUpload(OnClientS5Data(CCP, (ubyte *)DataPtr, DataSize));
		case CLIENT_STATE_S5_UDP_READY:
			return CCP->Profiler.MarkUpload(OnClientS5UdpData(CCP, (ubyte *)DataPtr, DataSize));

		case CLIENT_STATE_HTTP_WAIT_FOR_HEADER:
			return CCP->Profiler.MarkUpload(OnClientHttpHeader(CCP, DataPtr, DataSize));
		case CLIENT_STATE_HTTP_WAIT_FOR_AUTH_RESULT:
		case CLIENT_STATE_HTTP_AUTH_DONE:
		case CLIENT_STATE_HTTP_WAIT_FOR_DNS_RESULT:
		case CLIENT_STATE_HTTP_TCP_CONNECTING:
			return CCP->Profiler.MarkUpload(OnClientHttpBody(CCP, DataPtr, DataSize));
		case CLIENT_STATE_HTTP_TCP_ESTABLISHED:
			return CCP->Profiler.MarkUpload(OnClientHttpStream(CCP, DataPtr, DataSize));

		default:
			// X_DEBUG_PRINTF("CCP ID=%" PRIx64 ", STATE=%i\n%s", CCP->ClientConnectionId, (int)CCP->State, HexShow(DataPtr, DataSize).c_str());
			X_DEBUG_PRINTF("Ignored data: State:%i\n DataSize=%zi", (int)CCP->State, DataSize);
			break;
	}
	return InvalidDataSize;
}

size_t xProxyService::OnClientInit(xProxyClientConnection * CCP, void * DataPtr, size_t DataSize) {
	if (DataSize < 3) {
		return 0;  // not determinate
	}
	auto C0 = ((const ubyte *)DataPtr)[0];
	if (C0 == 0x05) {  // version : S5
		return OnClientS5Init(CCP, DataPtr, DataSize);
	}
	if (!isalpha(C0)) {
		return InvalidDataSize;
	}
	return OnClientHttpInit(CCP, DataPtr, DataSize);
}

void xProxyService::PostAuthRequest(xProxyClientConnection * CCP, const std::string_view AccountNameView, const std::string_view PasswordView) {
	auto Req          = xProxyClientAuth();
	Req.AddressString = CCP->GetRemoteAddress().ToString();
	Req.AccountName   = std::string(AccountNameView);
	Req.Password      = std::string(PasswordView);

	ubyte Buffer[MaxPacketSize];
	auto  RSize = WritePacket(Cmd_ProxyClientAuth, CCP->ClientConnectionId, Buffer, sizeof(Buffer), Req);
	if (!RSize) {
		return;
	}
	DispatcherClient.PostData(Buffer, RSize);
}

void xProxyService::PostDnsRequest(xProxyClientConnection * CCP, const std::string & Hostname) {
	auto Req     = xHostQueryReq();
	Req.Hostname = Hostname;

	ubyte Buffer[MaxPacketSize];
	auto  RSize = WritePacket(Cmd_HostQuery, CCP->ClientConnectionId, Buffer, sizeof(Buffer), Req);
	if (!RSize) {
		return;
	}
	DispatcherClient.PostData(Buffer, RSize);
}

void xProxyService::CreateTargetConnection(xProxyRelayClient * RCP, xProxyClientConnection * CCP) {
	xCreateRelayConnectionPair Req = {};
	Req.ClientConnectionId         = CCP->ClientConnectionId;
	Req.TerminalId                 = CCP->TerminalId;
	Req.TargetAddress              = CCP->TargetAddress;

	ubyte Buffer[MaxPacketSize];
	auto  RSize = WritePacket(Cmd_CreateConnection, 0, Buffer, sizeof(Buffer), Req);
	RCP->PostData(Buffer, RSize);
}

void xProxyService::DestroyTargetConnection(xIndexId ClientConnectionId) {
	auto CCP = ClientConnectionPool.CheckAndGet(ClientConnectionId);
	if (!CCP) {
		return;
	}
	DestroyTargetConnection(CCP);
}

void xProxyService::DestroyTargetConnection(xProxyClientConnection * CCP) {
	auto RelayClientPtr = RelayClientPool.CheckAndGet(CCP->TerminalControllerId);
	if (!RelayClientPtr) {
		X_DEBUG_PRINTF("Relay server missing");
		return;
	}

	auto Req             = xCloseRelayConnectionPair();
	Req.ConnectionPairId = CCP->ConnectionPairId;

	ubyte Buffer[MaxPacketSize];
	auto  RSize = WritePacket(Cmd_CloseConnection, CCP->ClientConnectionId, Buffer, sizeof(Buffer), Req);
	RelayClientPtr->PostData(Buffer, RSize);
}

void xProxyService::RequestUdpBinding(xProxyRelayClient * RCP, xProxyClientConnection * CCP) {
	auto Req               = xCreateUdpAssociation{};
	Req.ClientConnectionId = CCP->ClientConnectionId;
	Req.TerminalId         = CCP->TerminalId;

	ubyte Buffer[MaxPacketSize];
	auto  RSize = WritePacket(Cmd_CreateUdpAssociation, 0, Buffer, sizeof(Buffer), Req);
	RCP->PostData(Buffer, RSize);
}

void xProxyService::DestroyUdpBinding(xProxyClientConnection * CCP) {
	auto RelayClientPtr = RelayClientPool.CheckAndGet(CCP->TerminalControllerId);
	if (!RelayClientPtr) {
		X_DEBUG_PRINTF("Relay server missing");
		return;
	}

	auto Req             = xCloseUdpAssociation();
	Req.ConnectionPairId = CCP->ConnectionPairId;

	ubyte Buffer[MaxPacketSize];
	auto  RSize = WritePacket(Cmd_CloseUdpAssociation, CCP->ClientConnectionId, Buffer, sizeof(Buffer), Req);
	RelayClientPtr->PostData(Buffer, RSize);
}

xProxyRelayClient * xProxyService::GetTerminalController(xProxyClientConnection * CCP) {
	assert(!CCP->TerminalControllerId);
	auto PRC = GetTerminalController(CCP->TerminalControllerAddress);
	if (!PRC) {
		return nullptr;
	}
	CCP->TerminalControllerId = PRC->ConnectionId;
	return PRC;
}

xProxyRelayClient * xProxyService::GetTerminalController(const xNetAddress & Address) {
	auto AddressKey = Address.ToString();
	auto Iter       = RelayClientMap.find(AddressKey);
	if (Iter != RelayClientMap.end()) {  // found connection
		auto ConnectionId = Iter->second;
		assert(RelayClientPool.Check(ConnectionId));
		return &RelayClientPool[ConnectionId];
	}
	auto TerminalControllerId = RelayClientPool.Acquire();
	if (!TerminalControllerId) {
		X_DEBUG_PRINTF("Not relay client could be allocated");
		return nullptr;
	}
	auto PC = &RelayClientPool[TerminalControllerId];
	if (!PC->Init(this, Address)) {
		X_DEBUG_PRINTF("Failed to create connection to target address: %s", Address.ToString().c_str());
		RelayClientPool.Release(TerminalControllerId);
		return nullptr;
	}
	// new terminal controller id
	PC->ConnectionId           = TerminalControllerId;
	PC->LastDataTimestampMS    = NowMS;
	RelayClientMap[AddressKey] = TerminalControllerId;
	UpdateKeepAlive(PC);
	X_DEBUG_PRINTF("New connection to relay: %s", Address.ToString().c_str());
	return PC;
}

void xProxyService::PostDataToTerminalController(xProxyClientConnection * CCP, const void * _, size_t DataSize) {
	if (!DataSize) {  // life saver
		return;
	}

	auto DataPtr = (const ubyte *)_;
	auto PRC     = RelayClientPool.CheckAndGet(CCP->TerminalControllerId);
	if (!PRC) {
		X_DEBUG_PRINTF("Relay server disconnected");
		KillClientConnection(CCP);
		return;
	}
	assert(CCP->ConnectionPairId);
	ubyte  Buffer[MaxPacketSize];
	size_t RemainSize = DataSize;
	while (RemainSize) {
		auto Req             = xProxyToRelayData();
		auto PostSize        = std::min(RemainSize, MaxRelayPayloadSize);
		Req.ConnectionPairId = CCP->ConnectionPairId;
		Req.DataView         = { (const char *)DataPtr, PostSize };
		auto RSize           = WritePacket(Cmd_PostProxyToRelayData, 0, Buffer, sizeof(Buffer), Req);
		PRC->PostData(Buffer, RSize);

		DataPtr    += PostSize;
		RemainSize -= PostSize;
	}
	KeepAlive(CCP);
	UpdateAntiIdle(PRC);
	return;
}

void xProxyService::PostDataToTerminalControllerRaw(uint64_t TerminalControllerId, const void * DataPtr, size_t DataSize) {
	if (!DataSize) {  // life saver
		return;
	}
	auto PRC = RelayClientPool.CheckAndGet(TerminalControllerId);
	if (!PRC) {
		X_DEBUG_PRINTF("Relay server lost");
		return;
	}
	PRC->PostData(DataPtr, DataSize);
	UpdateAntiIdle(PRC);
	return;
}

void xProxyService::OnAuthResponse(uint64_t ClientConnectionId, const xProxyClientAuthResp & AuthResp) {
	auto CCP = ClientConnectionPool.CheckAndGet(ClientConnectionId);
	if (!CCP) {
		X_DEBUG_PRINTF("Connection not found, check request delay!");
		return;
	}
	switch (CCP->State) {
		case CLIENT_STATE_S5_WAIT_FOR_AUTH_RESULT: {
			X_DEBUG_PRINTF("S5 auth result, AuditId: %" PRIx64 "", AuthResp.AuditKey);
			OnS5AuthResult(CCP, AuthResp);
			return;
		}
		case CLIENT_STATE_HTTP_WAIT_FOR_AUTH_RESULT: {
			X_DEBUG_PRINTF("Http auth result, AuditId: %" PRIx64 "", AuthResp.AuditKey);
			OnHttpAuthResult(CCP, AuthResp);
			return;
		}
		default: {
			X_DEBUG_PRINTF("invalid auth result");
			KillClientConnection(CCP);
			break;
		}
	}
}

void xProxyService::OnDnsResponse(uint64_t ClientConnectionId, const xHostQueryResp & DnsResp) {
	auto CCP = ClientConnectionPool.CheckAndGet(ClientConnectionId);
	if (!CCP) {
		X_DEBUG_PRINTF("Connection not found, check request delay!");
		return;
	}
	switch (CCP->State) {
		case CLIENT_STATE_S5_WAIT_FOR_DNS_RESULT: {
			X_DEBUG_PRINTF("DNS Result: Ipv4=%s", DnsResp.Addr4.IpToString().c_str());
			auto TargetAddress = DnsResp.Addr4;
			TargetAddress.Port = CCP->TargetAddress.Port;  // restore port
			if (!DnsResp.Addr4) {
				X_DEBUG_PRINTF("invalid target address");
				KillClientConnection(CCP);
				return;
			}
			CCP->TargetAddress            = TargetAddress;
			auto RelayClientConnectionPtr = GetTerminalController(CCP);
			if (!RelayClientConnectionPtr) {
				X_DEBUG_PRINTF("Failed to establish connection to relay server");
				KillClientConnection(CCP);
				return;
			}
			CreateTargetConnection(RelayClientConnectionPtr, CCP);
			CCP->State = CLIENT_STATE_S5_TCP_CONNECTING;
			return;
		}
		case CLIENT_STATE_HTTP_WAIT_FOR_DNS_RESULT: {
			OnHttpDnsResult(CCP, DnsResp);
			return;
		}
		default: {
			X_DEBUG_PRINTF("invalid auth result");
			KillClientConnection(CCP);
			break;
		}
	}
}

void xProxyService::OnTerminalConnectionResult(const xCreateRelayConnectionPairResp & Result) {
	auto CCP = ClientConnectionPool.CheckAndGet(Result.ClientConnectionId);
	if (!CCP) {
		X_DEBUG_PRINTF("Missing source connection");
		return;
	}
	CCP->Profiler.MarkEstablished();
	switch (CCP->State) {
		case CLIENT_STATE_S5_TCP_CONNECTING: {
			if (!Result.ConnectionPairId) {
				static constexpr const ubyte S5Refused[] = {
					'\x05', '\x05', '\x00',          // refused
					'\x01',                          // ipv4
					'\x00', '\x00', '\x00', '\x00',  // ip: 0.0.0.0
					'\x00', '\x00',                  // port 0:
				};
				CCP->PostData(S5Refused, sizeof(S5Refused));
				FlushAndKillClientConnection(CCP);
				return;
			}
			static constexpr const ubyte S5Established[] = {
				'\x05', '\x00', '\x00',          // ok
				'\x01',                          // ipv4
				'\x00', '\x00', '\x00', '\x00',  // ip: 0.0.0.0
				'\x00', '\x00',                  // port 0:
			};
			CCP->PostData(S5Established, sizeof(S5Established));
			CCP->ConnectionPairId = Result.ConnectionPairId;
			CCP->State            = CLIENT_STATE_S5_TCP_ESTABLISHED;
			return;
		}
		case CLIENT_STATE_HTTP_TCP_CONNECTING: {
			OnHttpConnected(CCP, Result.ConnectionPairId);
			return;
		}
		default:
			X_DEBUG_PRINTF("Invalid client connection state, closing");
			KillClientConnection(CCP);
			break;
	}
}

void xProxyService::OnRelayData(const xRelayToProxyData & Post) {
	X_DEBUG_PRINTF("RelayData: ClientConnectionId=%" PRIx64 ", Size=%zi", Post.ClientConnectionId, Post.DataView.size());
	auto CCP = ClientConnectionPool.CheckAndGet(Post.ClientConnectionId);
	if (!CCP) {
		X_DEBUG_PRINTF("Connection lost");
		return;
	}
	auto DataView = Post.DataView;
	CCP->PostData(DataView.data(), DataView.size());
	KeepAlive(CCP);
}

void xProxyService::OnCloseConnection(const xCloseClientConnection & Post) {
	X_DEBUG_PRINTF("RelayClose");
	auto CCP = ClientConnectionPool.CheckAndGet(Post.ClientConnectionId);
	if (!CCP) {
		X_DEBUG_PRINTF("Connection lost");
		return;
	}
	FlushAndKillClientConnection(CCP);
}

void xProxyService::ShrinkAuthTimeout() {
	auto C = [KillTimepoint = NowMS - TCP_CONNECTION_AUTH_TIMEOUT_MS](const xProxyClientIdleNode & N) { return N.KeepAliveTimestampMS < KillTimepoint; };
	while (auto NP = ClientAuthTimeoutList.PopHead(C)) {
		KillClientConnection(NP);
	}
}

void xProxyService::ShrinkIdleTimeout() {
	auto C = [KillTimepoint = NowMS - TCP_CONNECTION_IDLE_TIMEOUT_MS](const xProxyClientIdleNode & N) { return N.KeepAliveTimestampMS < KillTimepoint; };
	while (auto NP = ClientIdleTimeoutList.PopHead(C)) {
		KillClientConnection(NP);
	}
}

void xProxyService::ShrinkFlushTimeout() {
	auto C = [KillTimepoint = NowMS - TCP_CONNECTION_FLUSH_TIMEOUT_MS](const xProxyClientIdleNode & N) { return N.KeepAliveTimestampMS < KillTimepoint; };
	while (auto NP = ClientFlushTimeoutList.PopHead(C)) {
		KillClientConnection(NP);
	}
}

void xProxyService::ShrinkKillList() {
	while (auto CCP = static_cast<xProxyClientConnection *>(ClientKillList.PopHead())) {
		X_DEBUG_PRINTF("Cleanup connection: %" PRIx64 "", CCP->ClientConnectionId);
		CCP->Profiler.MarkCloseConnection();
		do {                              // notify terminal to close connection
			if (CCP->ConnectionPairId) {  // active close connection
				X_DEBUG_PRINTF("ConnectionPairId: %" PRIx64 "", CCP->ConnectionPairId);
				if (CCP->State == CLIENT_STATE_S5_TCP_ESTABLISHED) {
					DestroyTargetConnection(CCP);
				} else if (CCP->State == CLIENT_STATE_S5_UDP_READY) {
					DestroyUdpBinding(CCP);
				}
			}
		} while (false);
		if (auto UdpReceiverId = Steal(CCP->LocalUdpChannelId)) {
			auto UP = ClientUdpChannelPool.CheckAndGet(UdpReceiverId);
			assert(UP);
			auto UCP = *UP;
			ClientUdpChannelPool.Release(UdpReceiverId);
			UCP->Clean();
			delete UCP;
		}
		ProfilerLogger.I("ClientConnection:%s", CCP->Profiler.Dump().ToString().c_str());
		CCP->Clean();
		assert(CCP == ClientConnectionPool.CheckAndGet(CCP->ClientConnectionId));
		ClientConnectionPool.Release(CCP->ClientConnectionId);
		--AuditClientConnectionCount;
	}
}

void xProxyService::ShrinkRelayClient() {
	auto RemoveRelayClient = [this](xProxyRelayClient * NP) {
		auto AddressKey = NP->GetTargetAddress().ToString();
		RelayClientMap.erase(AddressKey);
		NP->Clean();
		RelayClientPool.Release(NP->ConnectionId);
	};

	auto KeepAlivePoint = NowMS - MaxRelayKeepAliveTimeout;
	auto IdlePoint      = NowMS - MaxRelayClientIdleTimeout;
	auto Condition      = [KeepAlivePoint](const xProxyRelayClientNode & N) { return N.KeepAliveTimestampMS <= KeepAlivePoint; };
	while (auto NP = static_cast<xProxyRelayClient *>(RelayClientKeepAliveList.PopHead(Condition))) {
		if (NP->LastDataTimestampMS < IdlePoint) {
			RemoveRelayClient(NP);
			continue;
		}
		NP->PostRequestKeepAlive();
		UpdateKeepAlive(NP);
	}

	// Disconnected:
	while (auto NP = static_cast<xProxyRelayClient *>(RelayClientKillList.PopHead())) {
		RemoveRelayClient(NP);
	}
}
