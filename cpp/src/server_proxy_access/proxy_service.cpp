#include "../common_protocol/client_auth.hpp"
#include "../common_protocol/network.hpp"
#include "../common_protocol/terminal.hpp"
#include "./proxy_access.hpp"

// xProxyRelayClient
bool xProxyRelayClient::Init(xProxyService * ProxyServicePtr, const xNetAddress & TargetAddress) {
	if (!xTcpConnection::Init(ProxyServicePtr->IoCtxPtr, TargetAddress, this)) {
		return false;
	}
	this->ProxyServicePtr = ProxyServicePtr;
	return true;
}

void xProxyRelayClient::Clean() {
	xTcpConnection::Clean();
}

size_t xProxyRelayClient::OnData(xTcpConnection * TcpConnectionPtr, void * DataPtrInput, size_t DataSize) {
	assert(TcpConnectionPtr == this);
	auto   DataPtr    = static_cast<ubyte *>(DataPtrInput);
	size_t RemainSize = DataSize;
	while (RemainSize >= PacketHeaderSize) {
		auto Header = xPacketHeader::Parse(DataPtr);
		if (!Header) { /* header error */
			return InvalidDataSize;
		}
		auto PacketSize = Header.PacketSize;  // make a copy, so Header can be reused
		if (RemainSize < PacketSize) {        // wait for data
			break;
		}
		if (Header.IsKeepAlive()) {
			X_DEBUG_PRINTF("KeepAlive");
			ProxyServicePtr->KeepAlive(this);
		} else {
			auto PayloadPtr  = xPacket::GetPayloadPtr(DataPtr);
			auto PayloadSize = Header.GetPayloadSize();
			if (!OnPacket(Header, PayloadPtr, PayloadSize)) { /* packet error */
				return InvalidDataSize;
			}
		}
		DataPtr    += PacketSize;
		RemainSize -= PacketSize;
	}
	return DataSize - RemainSize;
}

bool xProxyRelayClient::OnPacket(const xPacketHeader & Header, ubyte * PayloadPtr, size_t PayloadSize) {
	X_DEBUG_PRINTF("CommandId: %" PRIu32 ", RequestId:%" PRIx64 ": \n%s", Header.CommandId, Header.RequestId, HexShow(PayloadPtr, PayloadSize).c_str());
	switch (Header.CommandId) {
		case Cmd_CreateConnectionResp: {
			auto Resp = xCreateTerminalConnectionResp();
			if (!Resp.Deserialize(PayloadPtr, PayloadSize)) {
				X_DEBUG_PRINTF("Invalid protocol");
				break;
			}
			ProxyServicePtr->OnTerminalConnectionResult(Resp);
			break;
		}
		default: {
			X_DEBUG_PRINTF("Unsupported packet");
		}
	}
	return true;
}

// xDispatcherClient
bool xProxyDispatcherClient::OnPacket(const xPacketHeader & Header, ubyte * PayloadPtr, size_t PayloadSize) {
	X_DEBUG_PRINTF("Dispatcher client CmdId=%" PRIx64 ":\n%s", Header.CommandId, HexShow(PayloadPtr, PayloadSize).c_str());
	switch (Header.CommandId) {
		case Cmd_ProxyClientAuthResp: {
			auto Resp = xProxyClientAuthResp();
			if (!Resp.Deserialize(PayloadPtr, PayloadSize)) {
				break;
			}
			ProxyServicePtr->OnAuthResponse(Header.RequestId, Resp);
			break;
		}
		case Cmd_HostQueryResp: {
			auto Resp = xHostQueryResp();
			if (!Resp.Deserialize(PayloadPtr, PayloadSize)) {
				break;
			}
			ProxyServicePtr->OnDnsResponse(Header.RequestId, Resp);
			break;
		}
		default:
			break;
	}
	return true;
}

// xProxyService

bool xProxyService::Init(xIoContext * IoCtxPtr, const xNetAddress & BindAddress, const xNetAddress & DispatcherAddress) {
	assert(IoCtxPtr && BindAddress && DispatcherAddress);
	RuntimeAssert(TcpServer.Init(IoCtxPtr, BindAddress, this, true));
	RuntimeAssert(DispatcherClient.Init(IoCtxPtr, DispatcherAddress, this));
	RuntimeAssert(RelayClientPool.Init(MAX_PROXY_RELAY_CONNECTION));
	RuntimeAssert(ClientConnectionPool.Init(MAX_PROXY_CLIENT_CONNECTION));

	this->IoCtxPtr = IoCtxPtr;
	NowMS          = GetTimestampMS();
	Reset(AuditClientConnectionCount);
	return true;
}

void xProxyService::Clean() {
	Renew(RelayClientMap);
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
	X_DEBUG_PRINTF("NewClientConnectionId:%" PRIx64 "", NewConnectionId);
	NewClientConnectionPtr->ClientConnectionId = NewConnectionId;
	NewClientConnectionPtr->TimestampMS        = NowMS;
	ClientAuthTimeoutList.AddTail(*NewClientConnectionPtr);
	++AuditClientConnectionCount;
}

size_t xProxyService::OnData(xTcpConnection * TcpConnectionPtr, void * DataPtr, size_t DataSize) {
	auto CCP = UpCast(TcpConnectionPtr);
	switch (CCP->State) {
		case CLIENT_STATE_INIT:
			return OnClientInit(CCP, DataPtr, DataSize);
		case CLIENT_STATE_S5_WAIT_FOR_CLIENT_AUTH:
			return OnClientS5Auth(CCP, DataPtr, DataSize);
		case CLIENT_STATE_S5_AUTH_DONE:
			return OnClientS5Connect(CCP, DataPtr, DataSize);
		default:
			X_DEBUG_PRINTF("Unprocessed data: \n%s", HexShow(DataPtr, DataSize).c_str());
			break;
	}
	return DataSize;
}

size_t xProxyService::OnClientInit(xProxyClientConnection * CCP, void * DataPtr, size_t DataSize) {
	if (DataSize < 3) {
		return 0;  // not determinate
	}
	if (((const ubyte *)DataPtr)[0] == 0x05) {  // version : S5
		X_DEBUG_PRINTF("S5 data: \n%s", HexShow(DataPtr, DataSize).c_str());

		xStreamReader R(DataPtr);
		R.Skip(1);                   // skip type check bytes
		size_t NM         = R.R1();  // number of methods
		size_t HeaderSize = 2 + NM;
		if (DataSize < HeaderSize) {
			return 0;  // not enough header size
		}
		bool UserPassSupport = false;
		for (size_t i = 0; i < NM; ++i) {
			uint8_t Method = R.R1();
			if (Method == 0x02) {
				UserPassSupport = true;
				break;
			}
		}
		ubyte Socks5Auth[2] = { 0x05, 0x00 };  // default: no auth
		if (UserPassSupport) {
			Socks5Auth[1] = 0x02;
			CCP->PostData(Socks5Auth, sizeof(Socks5Auth));
			CCP->State = CLIENT_STATE_S5_WAIT_FOR_CLIENT_AUTH;
			return HeaderSize;
		}
		CCP->PostData(Socks5Auth, sizeof(Socks5Auth));

		X_DEBUG_PRINTF("PostAuthByIp");
		PostAuthRequest(CCP, {}, {});
		CCP->State = CLIENT_STATE_S5_WAIT_FOR_AUTH_RESULT;
		return HeaderSize;
	}
	X_DEBUG_PRINTF("Non-socks5 protocol");
	return InvalidDataSize;
}

size_t xProxyService::OnClientS5Auth(xProxyClientConnection * CCP, void * DataPtr, size_t DataSize) {
	if (DataSize < 3) {
		return 0;
	}
	xStreamReader R(DataPtr);
	auto          Ver = R.R1();
	if (Ver != 0x01) {  // only version for user pass
		X_DEBUG_PRINTF("S5 NoAuth is not supported");
		return InvalidDataSize;
	}
	size_t NameLen = R.R1();
	if (DataSize < 3 + NameLen) {
		return 0;
	}
	auto   NameView = std::string_view((const char *)R.Skip(NameLen), NameLen);
	size_t PassLen  = R.R1();
	if (DataSize < 3 + NameLen + PassLen) {
		return 0;
	}
	auto PassView = std::string_view((const char *)R.Skip(PassLen), PassLen);
	PostAuthRequest(CCP, NameView, PassView);
	CCP->State = CLIENT_STATE_S5_WAIT_FOR_AUTH_RESULT;
	return R.Offset();
}

size_t xProxyService::OnClientS5Connect(xProxyClientConnection * CCP, void * DataPtr, size_t DataSize) {
	X_DEBUG_PRINTF("Request data: \n%s", HexShow(DataPtr, DataSize).c_str());

	if (DataSize < 10) {
		return 0;
	}
	if (DataSize >= 6 + 256) {
		X_DEBUG_PRINTF("Very big connection request, which is obviously wrong");
		return InvalidDataSize;
	}
	xStreamReader R(DataPtr);
	uint8_t       Version   = R.R();
	uint8_t       Operation = R.R();
	uint8_t       Reserved  = R.R();
	uint8_t       AddrType  = R.R();
	if (Version != 0x05 || Reserved != 0x00) {
		X_DEBUG_PRINTF("Non Socks5 connection request");
		return InvalidDataSize;
	}
	xNetAddress Address;
	char        DomainName[256];
	size_t      DomainNameLength = 0;
	if (AddrType == 0x01) {  // ipv4
		Address.Type = xNetAddress::IPV4;
		R.R(Address.SA4, 4);
		Address.Port = R.R2();
	} else if (AddrType == 0x04) {  // ipv6
		if (DataSize < 4 + 16 + 2) {
			return 0;
		}
		Address.Type = xNetAddress::IPV6;
		R.R(Address.SA6, 16);
		Address.Port = R.R2();
	} else if (AddrType == 0x03) {
		DomainNameLength = R.R();
		if (DataSize < 4 + 1 + DomainNameLength + 2) {
			return 0;
		}
		R.R(DomainName, DomainNameLength);
		DomainName[DomainNameLength] = '\0';
		Address.Port                 = R.R2();
	} else {
		X_DEBUG_PRINTF("invalid connection request type");
		return InvalidDataSize;
	}
	size_t AddressLength = R.Offset() - 3;
	if (Operation != 0x01 || AddrType == 0x06) {
		X_DEBUG_PRINTF("Operation other than tcp ipv4/domain connection");
		ubyte         Buffer[512];
		xStreamWriter W(Buffer);
		W.W(0x05);
		W.W(0x01);
		W.W(0x00);
		W.W((ubyte *)DataPtr + 3, AddressLength);
		CCP->PostData(Buffer, W.Offset());
		FlushAndKillClientConnection(CCP);
		return DataSize;
	}
	CCP->TargetAddress = Address;

	if (DomainNameLength) {  // dns query first
		CCP->State = CLIENT_STATE_S5_WAIT_FOR_DNS_RESULT;
		PostDnsRequest(CCP, { DomainName, DomainNameLength });
		return DataSize;
	}

	X_DEBUG_PRINTF("DirectConnect: %s", Address.IpToString().c_str());
	if (!MakeS5NewConnection(CCP)) {
		X_DEBUG_PRINTF("Failed to establish connection to relay server");
		return InvalidDataSize;
	}
	return DataSize;
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

void xProxyService::CreateTargetConnection(xProxyRelayClient * RCP, xIndexId ClientConnectionId, const xNetAddress & Target) {
	xCreateTerminalConnection Req = {};
	Req.SourceConnectionId        = ClientConnectionId;
	Req.TargetAddress             = Target;

	ubyte Buffer[MaxPacketSize];
	auto  RSize = WritePacket(Cmd_CreateConnection, 0, Buffer, sizeof(Buffer), Req);
	RCP->PostData(Buffer, RSize);
}

void xProxyService::DestroyTargetConnection(xIndexId SourceConnectionId) {
	auto CCP = ClientConnectionPool.CheckAndGet(SourceConnectionId);
	if (!CCP) {
		return;
	}
	FlushAndKillClientConnection(CCP);
}

xProxyRelayClient * xProxyService::MakeS5NewConnection(xProxyClientConnection * CCP) {
	CCP->State = CLIENT_STATE_S5_TCP_CONNECTING;
	assert(!CCP->TerminalControllerIndex);
	auto Key = CCP->TerminalControllerAddress.ToString();

	auto Iter = RelayClientMap.find(Key);
	if (Iter != RelayClientMap.end()) {  // found connection
		CCP->TerminalControllerIndex = Iter->second;
		assert(RelayClientPool.Check(CCP->TerminalControllerIndex));
		return &RelayClientPool[CCP->TerminalControllerIndex];
	}
	auto TerminalControllerIndex = RelayClientPool.Acquire();
	if (!TerminalControllerIndex) {
		X_DEBUG_PRINTF("Not relay client could be allocated");
		return nullptr;
	}
	auto PC = &RelayClientPool[TerminalControllerIndex];
	if (!PC->Init(this, CCP->TerminalControllerAddress)) {
		RelayClientPool.Release(TerminalControllerIndex);
		return nullptr;
	}
	//
	PC->ConnectionId         = TerminalControllerIndex;
	PC->KeepAliveTimestampMS = NowMS;
	PC->LastDataTimestampMS  = NowMS;
	RelayClientMap[Key]      = TerminalControllerIndex;
	return PC;
}

void xProxyService::OnAuthResponse(uint64_t ClientConnectionId, const xProxyClientAuthResp & AuthResp) {
	auto CCP = ClientConnectionPool.CheckAndGet(ClientConnectionId);
	if (!CCP) {
		X_DEBUG_PRINTF("Connection not found, check request delay!");
		return;
	}
	switch (CCP->State) {
		case CLIENT_STATE_S5_WAIT_FOR_AUTH_RESULT: {
			X_DEBUG_PRINTF("S5 auth result");
			if (!AuthResp.AuditKey) {
				X_DEBUG_PRINTF("No audit id found, close connection");
				CCP->State = CLIENT_STATE_S5_AUTH_FAILED;
				CCP->PostData("\x01\x01", 2);
				FlushAndKillClientConnection(CCP);
				return;
			}
			CCP->TerminalControllerAddress  = AuthResp.TerminalControllerAddress;
			CCP->TerminalControllerSubIndex = AuthResp.TerminalControllerSubIndex;
			CCP->State                      = CLIENT_STATE_S5_AUTH_DONE;
			CCP->PostData("\x01\x00", 2);
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
			auto RelayClientConnectionPtr = MakeS5NewConnection(CCP);
			if (!RelayClientConnectionPtr) {
				X_DEBUG_PRINTF("Failed to establish connection to relay server");
				KillClientConnection(CCP);
				return;
			}
			CreateTargetConnection(RelayClientConnectionPtr, CCP->ClientConnectionId, TargetAddress);
			CCP->State = CLIENT_STATE_S5_TCP_CONNECTING;
			return;
		}
		default: {
			X_DEBUG_PRINTF("invalid auth result");
			KillClientConnection(CCP);
			break;
		}
	}
}

void xProxyService::OnTerminalConnectionResult(const xCreateTerminalConnectionResp & Result) {
	auto CCP = ClientConnectionPool.CheckAndGet(Result.SourceConnectionId);
	if (!CCP) {
		X_DEBUG_PRINTF("Missing source connection");
		return;
	}
	switch (CCP->State) {
		case CLIENT_STATE_S5_TCP_CONNECTING: {
			if (!Result.TerminalTargetConnectionId) {
				static constexpr const ubyte S5Refused[] = {
					'\x05', '\x05', '\x00',          // refused
					'\x01',                          // ipv4
					'\x00', '\x00', '\x00', '\x00',  // ip: 0.0.0.0
					'\x00', '\x00',                  // port 0:
				};
				CCP->PostData(S5Refused, sizeof(S5Refused));
				FlushAndKillClientConnection(CCP);
			}
			static constexpr const ubyte S5Established[] = {
				'\x05', '\x00', '\x00',          // ok
				'\x01',                          // ipv4
				'\x00', '\x00', '\x00', '\x00',  // ip: 0.0.0.0
				'\x00', '\x00',                  // port 0:
			};
			CCP->PostData(S5Established, sizeof(S5Established));
			CCP->State = CLIENT_STATE_S5_TCP_ESTABLISHED;
			break;
		}
		default:
			X_DEBUG_PRINTF("Invalid client connection state, closing");
			KillClientConnection(CCP);
	}
}

void xProxyService::ShrinkAuthTimeout() {
	auto KillTimepoint = NowMS - TCP_CONNECTION_AUTH_TIMEOUT_MS;
	for (auto & N : ClientAuthTimeoutList) {
		if (N.TimestampMS > KillTimepoint) {
			break;
		}
		KillClientConnection(&N);
	}
}

void xProxyService::ShrinkIdleTimeout() {
	auto KillTimepoint = NowMS - TCP_CONNECTION_IDLE_TIMEOUT_MS;
	for (auto & N : ClientIdleTimeoutList) {
		if (N.TimestampMS > KillTimepoint) {
			break;
		}
		KillClientConnection(&N);
	}
}

void xProxyService::ShrinkFlushTimeout() {
	auto KillTimepoint = NowMS - TCP_CONNECTION_FLUSH_TIMEOUT_MS;
	for (auto & N : ClientFlushTimeoutList) {
		if (N.TimestampMS > KillTimepoint) {
			break;
		}
		KillClientConnection(&N);
	}
}

void xProxyService::ShrinkKillList() {
	for (auto & N : ClientKillList) {
		auto ClientConnectionPtr = static_cast<xProxyClientConnection *>(&N);
		ClientConnectionPtr->Clean();
		assert(ClientConnectionPtr == ClientConnectionPool.CheckAndGet(ClientConnectionPtr->ClientConnectionId));
		ClientConnectionPool.Release(ClientConnectionPtr->ClientConnectionId);
		--AuditClientConnectionCount;
	}
}

void xProxyService::ShrinkRelayClient() {
	// TODO: KeepAlive
	// TODO: Close idle
}
