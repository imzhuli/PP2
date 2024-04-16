#include "./proxy_access.hpp"

size_t xProxyService::OnClientS5Init(xProxyClientConnection * CCP, void * DataPtr, size_t DataSize) {
	X_DEBUG_PRINTF("New s5 connection");

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
	X_DEBUG_PRINTF("Request: \n%s", HexShow(DataPtr, DataSize).c_str());

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
	if (AddrType == 0x06) {
		goto UNSUPPORTED;
	}
	CCP->TargetAddress = Address;

	if (Operation == 0x01) {     // tcp connection:
		if (DomainNameLength) {  // dns query first
			X_DEBUG_PRINTF("Client s5 wait for dns result");
			CCP->State = CLIENT_STATE_S5_WAIT_FOR_DNS_RESULT;
			PostDnsRequest(CCP, { DomainName, DomainNameLength });
			return DataSize;
		}

		X_DEBUG_PRINTF("DirectConnect: %s", Address.ToString().c_str());
		auto RelayClientConnectionPtr = GetTerminalController(CCP);
		if (!RelayClientConnectionPtr) {
			X_DEBUG_PRINTF("Failed to establish connection to relay server");
			KillClientConnection(CCP);
			return InvalidDataSize;
		}
		CreateTargetConnection(RelayClientConnectionPtr, CCP);
		CCP->State = CLIENT_STATE_S5_TCP_CONNECTING;
		return DataSize;
	}

	if (Operation == 0x03) {     // udp bind address
		if (DomainNameLength) {  // udp bind refuse domain name
			goto UNSUPPORTED;
		}
		if (!CreateLocalUdpChannel(CCP)) {
			goto LOCAL_UDP_FAILED;
		}
		auto RelayClientConnectionPtr = GetTerminalController(CCP);
		RequestUdpBinding(RelayClientConnectionPtr, CCP);
		CCP->State = CLIENT_STATE_S5_UDP_BINDING;
		return DataSize;
	}

LOCAL_UDP_FAILED:
UNSUPPORTED:
	ubyte         Buffer[512];
	xStreamWriter W(Buffer);
	W.W(0x05);
	W.W(0x01);
	W.W(0x00);
	W.W((ubyte *)DataPtr + 3, AddressLength);
	CCP->PostData(Buffer, W.Offset());
	CCP->State = CLIENT_STATE_CLOSED;
	FlushAndKillClientConnection(CCP);
	return DataSize;
}

size_t xProxyService::OnClientS5Data(xProxyClientConnection * CCP, ubyte * DataPtr, size_t DataSize) {
	PostDataToTerminalController(CCP, DataPtr, DataSize);
	KeepAlive(CCP);
	return DataSize;
}

void xProxyService::OnS5AuthResult(xProxyClientConnection * CCP, const xProxyClientAuthResp & AuthResp) {
	if (!AuthResp.AuditKey) {
		X_DEBUG_PRINTF("No audit id found, close connection");
		CCP->State = CLIENT_STATE_CLOSED;
		CCP->PostData("\x01\x01", 2);
		FlushAndKillClientConnection(CCP);
		return;
	}
	CCP->TerminalControllerAddress = AuthResp.TerminalControllerAddress;
	CCP->TerminalId                = AuthResp.TerminalId;
	CCP->State                     = CLIENT_STATE_S5_AUTH_DONE;
	CCP->PostData("\x01\x00", 2);
	return;
}

size_t xProxyService::OnClientS5UdpData(xProxyClientConnection * CCP, ubyte * DataPtr, size_t DataSize) {
	X_DEBUG_PRINTF("Ignored UdpData: \n%s", HexShow(DataPtr, DataSize).c_str());
	return DataSize;
}

bool xProxyService::CreateLocalUdpChannel(xProxyClientConnection * CCP) {
	assert(!CCP->LocalUdpChannelId);
	auto P = new (std::nothrow) xProxyUdpReceiver();
	if (!P) {
		return false;
	}
	auto Id = ClientUdpChannelPool.Acquire(P);
	if (!Id) {
		delete P;
		return false;
	}
	auto BindAddress = CCP->GetLocalAddress().Decay();
	if (!P->Init(this->IoCtxPtr, BindAddress, this)) {
		ClientUdpChannelPool.Release(Id);
		delete P;
		return false;
	}
	P->ClientConnectionId  = CCP->ClientConnectionId;
	CCP->LocalUdpChannelId = Id;
	return true;
}

void xProxyService::OnTerminalUdpAssociationResult(const xCreateUdpAssociationResp & Result) {
	auto CCP = ClientConnectionPool.CheckAndGet(Result.ClientConnectionId);
	if (!CCP) {
		X_DEBUG_PRINTF("Missing source connection");
		return;
	}
	X_DEBUG_PRINTF("PairId: %" PRIx64 "", Result.ConnectionPairId);
	switch (CCP->State) {
		case CLIENT_STATE_S5_UDP_BINDING: {
			assert(CCP->LocalUdpChannelId);
			auto UP  = ClientUdpChannelPool.CheckAndGet(CCP->LocalUdpChannelId);
			auto URP = UP ? *UP : nullptr;
			if (!URP) {
				DestroyUdpBinding(CCP);
			}
			if (!URP || !Result.ConnectionPairId) {
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
			auto & Address         = URP->GetBindAddress();
			ubyte  S5Established[] = {
                '\x05', '\x00', '\x00',          // ok
                '\x01',                          // ipv4
                '\x00', '\x00', '\x00', '\x00',  // ip: 0.0.0.0
                '\x00', '\x00',                  // port 0:
			};
			auto W = xStreamWriter(S5Established + 4);
			W.W(Address.SA4, 4);
			W.W2(Address.Port);

			CCP->PostData(S5Established, sizeof(S5Established));
			CCP->ConnectionPairId = Result.ConnectionPairId;
			CCP->State            = CLIENT_STATE_S5_UDP_READY;
			return;
		}
		default:
			X_DEBUG_PRINTF("Invalid client connection state, closing");
			KillClientConnection(CCP);
			break;
	}
}

void xProxyService::OnData(xUdpChannel * ChannelPtr, void * DataPtr, size_t DataSize, const xNetAddress & RemoteAddress) {
	X_DEBUG_PRINTF("UDP Data: @%s\n%s", RemoteAddress.ToString().c_str(), HexShow(DataPtr, DataSize).c_str());
}
