#include "./proxy_access.hpp"

// return value: consumed size, non-zero only on success
size_t ParseS5UdpAddress(xNetAddress & Output, const void * StartPtr, size_t BufferSize) {
	auto R                 = xStreamReader(StartPtr);
	auto TotalRequiredSize = (size_t)4;
	if (BufferSize < TotalRequiredSize) {
		return 0;
	}
	R.Skip(2);  // RESERVED
	auto Frag = R.R1();
	if (Frag) {
		X_DEBUG_PRINTF("udp fragment relaying is not supported");
		return 0;
	}
	auto Type = R.R1();
	if (Type == 0x01) {          // IPV4
		TotalRequiredSize += 6;  // address + port
		if (BufferSize < TotalRequiredSize) {
			return 0;
		}
		Output = xNetAddress::Make4();
		R.R(Output.SA4, 4);
	} else if (Type == 0x06) {
		TotalRequiredSize += 18;  // address + port
		if (BufferSize < TotalRequiredSize) {
			return 0;
		}
		Output = xNetAddress::Make6();
		R.R(Output.SA6, 16);
	} else {
		X_DEBUG_PRINTF("Unsupported relay address type");
		return 0;
	}
	Output.Port = R.R2();
	return R.Offset();
}

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
			X_DEBUG_PRINTF("ignore hostname in udp binding");
		}
		if (0 == (CCP->ClientFlags & xProxyClientConnection::CLIENT_FLAG_ENABLE_UDP)) {
			X_DEBUG_PRINTF("Not authorized client");
			goto LOCAL_UDP_FAILED;
		}
		if (!CreateLocalUdpChannel(CCP)) {
			X_DEBUG_PRINTF("Failed to create local udp channel");
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
	if (!AuthResp.AuditKey || !AuthResp.TerminalControllerAddress) {
		X_DEBUG_PRINTF("No audit id found, close connection");
		CCP->State = CLIENT_STATE_CLOSED;
		CCP->PostData("\x01\x01", 2);
		FlushAndKillClientConnection(CCP);
		return;
	}
	X_DEBUG_PRINTF("TerminalControllerBinding: %s, %" PRIx64 "", AuthResp.TerminalControllerAddress.ToString().c_str(), AuthResp.TerminalId);
	CCP->TerminalControllerAddress = AuthResp.TerminalControllerAddress;
	CCP->TerminalId                = AuthResp.TerminalId;
	CCP->ClientFlags               = AuthResp.EnableUdp ? xProxyClientConnection::CLIENT_FLAG_ENABLE_UDP : 0;
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
	auto URP = new (std::nothrow) xProxyUdpReceiver();
	if (!URP) {
		return false;
	}
	auto Id = ClientUdpChannelPool.Acquire(URP);
	if (!URP) {
		delete URP;
		return false;
	}
	auto BindAddress = CCP->GetLocalAddress().Decay();
	if (!URP->Init(this->IoCtxPtr, BindAddress, this)) {
		ClientUdpChannelPool.Release(Id);
		delete URP;
		return false;
	}
	URP->ClientConnectionId = CCP->ClientConnectionId;
	CCP->LocalUdpChannelId  = Id;
	KeepAlive(URP);
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
				X_DEBUG_PRINTF("Local udp channal not found!");
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
			KeepAliveUseUdpTimeout(CCP);
			URP->TerminalControllerId = CCP->TerminalControllerId;
			URP->ConnectionPairId     = Result.ConnectionPairId;

			auto & Address         = URP->GetBindAddress();
			ubyte  S5Established[] = {
                '\x05', '\x00', '\x00',          // ok
                '\x01',                          // ipv4
                '\x00', '\x00', '\x00', '\x00',  // ip: 0.0.0.0
                '\x00', '\x00',                  // port 0:
			};
			auto W = xStreamWriter(S5Established + 4);
			W.W(UdpExportAddress.SA4, 4);
			W.W2(Address.Port);

			X_DEBUG_PRINTF("Response: %s", StrToHex(S5Established, sizeof(S5Established)).c_str());

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

void xProxyService::OnData(xUdpChannel * ChannelPtr, void * _, size_t DataSize, const xNetAddress & RemoteAddress) {
	auto URP       = static_cast<xProxyUdpReceiver *>(ChannelPtr);
	auto DataPtr   = static_cast<ubyte *>(_);
	auto ToAddress = xNetAddress();
	auto ASize     = ParseS5UdpAddress(ToAddress, DataPtr, DataSize);
	if (!ASize) {
		X_DEBUG_PRINTF("Invalid udp target address");
		return;
	}
	DataSize -= ASize;
	if (!DataSize) {
		X_DEBUG_PRINTF("Invalid data size");
		return;
	}
	DataPtr += ASize;
	X_DEBUG_PRINTF("Udp Data: %s -> %s\n%s", RemoteAddress.ToString().c_str(), ToAddress.ToString().c_str(), HexShow(DataPtr, DataSize).c_str());

	ubyte Buffer[MaxPacketSize];
	auto  R            = xProxyToRelayUdpData();
	R.ConnectionPairId = URP->ConnectionPairId;
	R.ToAddress        = ToAddress;
	R.DataView         = { (const char *)DataPtr, DataSize };
	auto RSize         = WritePacket(Cmd_PostProxyToRelayUdpData, 0, Buffer, sizeof(Buffer), R);

	PostDataToTerminalControllerRaw(URP->TerminalControllerId, Buffer, RSize);
	URP->LastSourceAddress = RemoteAddress;
	return;
}

void xProxyService::OnRelayUdpData(const xRelayToProxyUdpData & Post) {
	X_DEBUG_PRINTF("RelayData: ClientConnectionId=%" PRIx64 ", Size=%zi, \n%s", Post.ClientConnectionId, Post.DataView.size(), HexShow(Post.DataView).c_str());
	auto CCP = ClientConnectionPool.CheckAndGet(Post.ClientConnectionId);
	if (!CCP) {
		X_DEBUG_PRINTF("Connection lost");
		return;
	}
	assert(CCP->LocalUdpChannelId);
	auto UP  = ClientUdpChannelPool.CheckAndGet(CCP->LocalUdpChannelId);
	auto URP = UP ? *UP : nullptr;
	if (!URP) {
		X_DEBUG_PRINTF("Local udp channal not found!");
		return;
	}
	assert(Post.DataView.size() <= MaxRelayPayloadSize);

	if (!Post.FromAddress.IsV4()) {
		X_DEBUG_PRINTF("Unsupported address type");
		return;
	}

	ubyte Buffer[MaxPacketSize];
	auto  W = xStreamWriter(Buffer);
	W.W2(0);     // reserved
	W.W1(0);     // no frag
	W.W1(0x01);  // ipv4
	W.W(Post.FromAddress.SA4, 4);
	W.W2(Post.FromAddress.Port);
	W.W(Post.DataView.data(), Post.DataView.size());

	X_DEBUG_PRINTF(
		"Post data %s -> %s \n%s", Post.FromAddress.ToString().c_str(), URP->LastSourceAddress.ToString().c_str(), HexShow(Buffer, W.Offset()).c_str()
	);
	URP->PostData(Buffer, W.Offset(), URP->LastSourceAddress);
	return;
}