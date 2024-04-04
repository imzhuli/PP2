#include "../common_protocol/client_auth.hpp"
#include "./proxy_access.hpp"

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
