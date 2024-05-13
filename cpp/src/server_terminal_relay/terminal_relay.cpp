#include "./terminal_relay.hpp"

namespace {
	[[maybe_unused]] constexpr const uint64_t CONNECTION_PAIR_FLAG_UDP = 0x01;

	[[maybe_unused]] inline bool IsTcp(xRelayConnectionPair * RTP) {
		return !(RTP->UserFlags & CONNECTION_PAIR_FLAG_UDP);
	}
	[[maybe_unused]] inline bool IsUdp(xRelayConnectionPair * RTP) {
		return (RTP->UserFlags & CONNECTION_PAIR_FLAG_UDP);
	}
	[[maybe_unused]] inline bool SetUdp(xRelayConnectionPair * RTP) {
		return RTP->UserFlags |= CONNECTION_PAIR_FLAG_UDP;
	}

}  // namespace

///////////////

bool xRelayUdpChannel::Init(xTerminalRelay * RelayPtr, const xNetAddress & BindAddress, uint64_t RelayConnectionPairId) {
	if (!UdpChannel.Init(RelayPtr->GetIoCtxPtr(), BindAddress, this)) {
		return false;
	}
	this->RelayPtr              = RelayPtr;
	this->RelayConnectionPairId = RelayConnectionPairId;
	return true;
}

void xRelayUdpChannel::Clean() {
	UdpChannel.Clean();
}

////////////////

void xRelayUdpChannel::OnData(xUdpChannel * ChannelPtr, void * DataPtr, size_t DataSize, const xNetAddress & RemoteAddress) {
	X_DEBUG_PRINTF("DataFrom:%s Data=\n%s", RemoteAddress.ToString().c_str(), HexShow(DataPtr, DataSize).c_str());
	RelayPtr->OnTargetUdpData(this, DataPtr, DataSize, RemoteAddress);
}

bool xRelayTerminalConnection::Init(
	xTerminalRelay * RelayPtr, const xNetAddress & TargetAddress, const xNetAddress & BindAddress, uint64_t RelayConnectionPairId
) {
	if (TargetAddress.Type != BindAddress.Type) {
		X_DEBUG_PRINTF("Invalid bind address type");
		return false;
	}

	auto IoCtxPtr = RelayPtr->GetIoCtxPtr();
	if (!Connection.Init(IoCtxPtr, TargetAddress, BindAddress, this)) {
		X_DEBUG_PRINTF("Failed to create connection");
		return false;
	}
	this->RelayPtr              = RelayPtr;
	this->RelayConnectionPairId = RelayConnectionPairId;
	return true;
}

void xRelayTerminalConnection::Clean() {
	Connection.Clean();
}

void xRelayTerminalConnection::OnConnected(xTcpConnection * TcpConnectionPtr) {
	RelayPtr->OnTargetConnectionEstablished(this);
}

size_t xRelayTerminalConnection::OnData(xTcpConnection * TcpConnectionPtr, void * DataPtr, size_t DataSize) {
	RelayPtr->OnTargetConnectionData(this, (ubyte *)DataPtr, DataSize);
	return DataSize;
}

void xRelayTerminalConnection::OnPeerClose(xTcpConnection * TcpConnectionPtr) {
	RelayPtr->OnTargetConnectionClosed(this);
}

////////////////

bool xTerminalRelay::Init(xIoContext * IoCtxPtr, const xNetAddress & BindAddress, const char * TerminalBindingFile) {
	RuntimeAssert(xTerminalController::Init(IoCtxPtr, BindAddress));
	RuntimeAssert(LoadTerminalBindings(TerminalBindingFile));
	this->IoCtxPtr = IoCtxPtr;
	return true;
}

void xTerminalRelay::Clean() {
	xTerminalController::Clean();
}

void xTerminalRelay::Tick(uint64_t NowMS) {
	xTerminalController::Tick(NowMS);
	while (auto RTP = static_cast<xRelayTerminalConnection *>(KillConnectionList.PopHead())) {
		RTP->Clean();
		delete RTP;
	}
	while (auto RUC = static_cast<xRelayUdpChannel *>(KillUdpAssociationList.PopHead())) {
		RUC->Clean();
		delete RUC;
	}
}

bool xTerminalRelay::LoadTerminalBindings(const char * TerminalBindingFile) {
	auto Map = LoadStaticIpTable(TerminalBindingFile);
	for (auto & N : Map) {
		auto & R = N.second;
		if (!R.TargetIp) {
			continue;
		}
		BindIpTable[R.TerminalId] = R.TargetIp;
	}
	return !BindIpTable.empty();
}

xNetAddress xTerminalRelay::GetBindAddress(uint64_t TerminalId) {
	auto Iter = BindIpTable.find(TerminalId);
	if (Iter == BindIpTable.end()) {
		return {};
	}
	return Iter->second;
}

void xTerminalRelay::OnClientConnected(xServiceClientConnection & Connection) {
	Connection.ResizeSendBuffer(1'500'000);
	Connection.ResizeRecvBuffer(1'500'000);
}

void xTerminalRelay::OnClientClose(xServiceClientConnection & Connection) {
}

bool xTerminalRelay::OnPacket(xServiceClientConnection & Connection, const xPacketHeader & Header, ubyte * PayloadPtr, size_t PayloadSize) {
	switch (Header.CommandId) {
		case Cmd_CreateConnection:
			OnProxyCreateConnection(Connection, Header, PayloadPtr, PayloadSize);
			break;
		case Cmd_CloseConnection:
			OnProxyCloseConnection(Connection, Header, PayloadPtr, PayloadSize);
			break;
		case Cmd_PostProxyToRelayData:
			OnProxyToRelay(Connection, Header, PayloadPtr, PayloadSize);
			break;
		case Cmd_CreateUdpAssociation:
			OnProxyCreateUdpAssociation(Connection, Header, PayloadPtr, PayloadSize);
			break;
		case Cmd_CloseUdpAssociation:
			OnProxyCloseUdpAssociation(Connection, Header, PayloadPtr, PayloadSize);
			break;
		case Cmd_PostProxyToRelayUdpData:
			OnProxyToRelayUdp(Connection, Header, PayloadPtr, PayloadSize);
			break;
		default:
			X_DEBUG_PRINTF("OnPacket: %" PRIx32 ", rid=%" PRIx64 "", Header.CommandId, Header.RequestId);
			break;
	}
	return true;
}

void xTerminalRelay::OnProxyCreateConnection(xServiceClientConnection & Connection, const xPacketHeader & Header, ubyte * PayloadPtr, size_t PayloadSize) {
	auto Req = xCreateRelayConnectionPair();
	if (!Req.Deserialize(PayloadPtr, PayloadSize)) {
		X_PERROR("Invalid protocol");
		return;
	}
	auto Resp               = xCreateRelayConnectionPairResp();
	Resp.ClientConnectionId = Req.ClientConnectionId;

	ubyte Buffer[MaxPacketSize];
	auto  PP = CreateConnectionPair();
	if (!PP) {
		auto RSize = WritePacket(Cmd_CreateConnectionResp, Header.RequestId, Buffer, sizeof(Buffer), Resp);
		Connection.PostData(Buffer, RSize);
		return;
	}
	auto PPG = xScopeGuard([&, this] { DestroyConnectionPair(PP); });

	auto RTP         = new xRelayTerminalConnection();
	auto BindAddress = GetBindAddress(Req.TerminalId);
	X_DEBUG_PRINTF("Trying to create connection: bind@%s, Target@%s", BindAddress.ToString().c_str(), Req.TargetAddress.ToString().c_str());
	if (!BindAddress || !Req.TargetAddress || !RTP->Init(this, Req.TargetAddress, BindAddress, PP->ConnectionPairId)) {
		X_DEBUG_PRINTF("Failed to create terminal connection");
		auto RSize = WritePacket(Cmd_CreateConnectionResp, Header.RequestId, Buffer, sizeof(Buffer), Resp);
		Connection.PostData(Buffer, RSize);
		delete RTP;
		return;
	}
	auto RTPG = xScopeGuard([&] {
		RTP->Clean();
		delete RTP;
	});

	PP->ProxyConnectionId  = Connection.GetConnectionId();
	PP->ClientConnectionId = Req.ClientConnectionId;
	SetTargetConnection(PP, RTP);

	PPG.Dismiss();
	RTPG.Dismiss();
	return;
}

void xTerminalRelay::OnProxyCloseConnection(xServiceClientConnection & Connection, const xPacketHeader & Header, ubyte * PayloadPtr, size_t PayloadSize) {
	auto Req = xCloseRelayConnectionPair();
	if (!Req.Deserialize(PayloadPtr, PayloadSize)) {
		X_PERROR("Invalid protocol");
		return;
	}
	auto PP = GetConnectionPairById(Req.ConnectionPairId);
	if (!PP) {
		X_DEBUG_PRINTF("Connection lost");
		return;
	}
	X_DEBUG_PRINTF("OnCloseConnection: ConnectionPairId=%" PRIx64 ", FoundPP=%s", Req.ConnectionPairId, YN(PP));
	auto RTP = GetTargetConnection(PP);
	assert(RTP);
	KillConnection(RTP);
	return;
}

void xTerminalRelay::OnProxyToRelay(xServiceClientConnection & Connection, const xPacketHeader & Header, ubyte * PayloadPtr, size_t PayloadSize) {
	auto Req = xProxyToRelayData();
	if (!Req.Deserialize(PayloadPtr, PayloadSize)) {
		return;
	}

	auto PP = GetConnectionPairById(Req.ConnectionPairId);
	if (!PP) {
		X_DEBUG_PRINTF("Failed to find connection pair: Id=%" PRIx64 "", Req.ConnectionPairId);
		return;
	}
	KeepAlive(PP);

	auto RTP = GetTargetConnection(PP);
	if (!RTP) {
		X_DEBUG_PRINTF("Target connection lost");
		return;
	}
	RTP->Connection.PostData(Req.DataView.data(), Req.DataView.size());
	return;
}

void xTerminalRelay::OnProxyCreateUdpAssociation(xServiceClientConnection & Connection, const xPacketHeader & Header, ubyte * PayloadPtr, size_t PayloadSize) {
	auto Req = xCreateUdpAssociation();
	if (!Req.Deserialize(PayloadPtr, PayloadSize)) {
		X_PERROR("Invalid protocol");
		return;
	}
	auto BindAddress = GetBindAddress(Req.TerminalId);
	X_DEBUG_PRINTF(
		"Udp association: ClientConnectionId=%" PRIx64 ", TerminalId=%" PRIx64 ", BindAddressHint=%s", Req.ClientConnectionId, Req.TerminalId,
		BindAddress.ToString().c_str()
	);

	auto Resp               = xCreateUdpAssociationResp();
	Resp.ClientConnectionId = Req.ClientConnectionId;

	ubyte Buffer[MaxPacketSize];
	auto  PP = CreateConnectionPair();
	if (!PP) {
		auto RSize = WritePacket(Cmd_CreateUdpAssociationResp, Header.RequestId, Buffer, sizeof(Buffer), Resp);
		Connection.PostData(Buffer, RSize);
		return;
	}
	auto PPG = xScopeGuard([&, this] { DestroyConnectionPair(PP); });

	// real connection object:
	auto RUC = new xRelayUdpChannel();
	if (!RUC->Init(this, BindAddress, PP->ConnectionPairId)) {
		auto RSize = WritePacket(Cmd_CreateUdpAssociationResp, Header.RequestId, Buffer, sizeof(Buffer), Resp);
		Connection.PostData(Buffer, RSize);
		delete RUC;
		return;
	}
	auto RUCG = xScopeGuard([&] {
		RUC->Clean();
		delete RUC;
	});

	PP->ProxyConnectionId  = Connection.GetConnectionId();
	PP->ClientConnectionId = Req.ClientConnectionId;
	SetUdpAssociation(PP, RUC);

	// post success:
	Resp.ConnectionPairId = PP->ConnectionPairId;
	auto RSize            = WritePacket(Cmd_CreateUdpAssociationResp, Header.RequestId, Buffer, sizeof(Buffer), Resp);
	Connection.PostData(Buffer, RSize);

	PPG.Dismiss();
	RUCG.Dismiss();
	return;
}

void xTerminalRelay::OnProxyCloseUdpAssociation(xServiceClientConnection & Connection, const xPacketHeader & Header, ubyte * PayloadPtr, size_t PayloadSize) {
	auto Req = xCloseUdpAssociation();
	if (!Req.Deserialize(PayloadPtr, PayloadSize)) {
		X_PERROR("Invalid protocol");
		return;
	}
	auto PP = GetConnectionPairById(Req.ConnectionPairId);
	if (!PP) {
		X_DEBUG_PRINTF("UdpAssociation lost");
		return;
	}
	KillConnectionPair(PP);
}

void xTerminalRelay::OnProxyToRelayUdp(xServiceClientConnection & Connection, const xPacketHeader & Header, ubyte * PayloadPtr, size_t PayloadSize) {
	auto Req = xProxyToRelayUdpData();
	if (!Req.Deserialize(PayloadPtr, PayloadSize)) {
		X_PERROR("Invalid protocol");
		return;
	}
	auto PP = GetConnectionPairById(Req.ConnectionPairId);
	if (!PP) {
		X_DEBUG_PRINTF("UdpAssociation PairId=%", PRIx64 "", Req.ConnectionPairId);
		return;
	}
	KeepAlive(PP);
	auto UP = GetUdpAssociation(PP);
	if (!UP) {
		X_DEBUG_PRINTF("UdpAssociation lost: PairId=%" PRIx64 "", PP->ConnectionPairId);
		return;
	}
	X_DEBUG_PRINTF("UdpRequest: @%s\n%s", Req.ToAddress.ToString().c_str(), HexShow(Req.DataView).c_str());
	UP->UdpChannel.PostData(Req.DataView.data(), Req.DataView.size(), Req.ToAddress);
}

/**************/

void xTerminalRelay::OnDestroyTimeoutConnectionPair(xRelayConnectionPair * CP) {
	if (IsTcp(CP)) {
		auto RTP = static_cast<xRelayTerminalConnection *>(CP->UserCtx.P);
		RTP->Clean();
		delete RTP;
	}
	if (IsUdp(CP)) {
		auto RUCP = static_cast<xRelayUdpChannel *>(CP->UserCtx.P);
		RUCP->Clean();
		delete RUCP;
	}
}

void xTerminalRelay::OnTargetConnectionEstablished(xRelayTerminalConnection * RTP) {
	auto PP = GetConnectionPairById(RTP->RelayConnectionPairId);
	if (!PP) {
		X_DEBUG_PRINTF("Connection lost");
		return;
	}
	assert(PP->ProxyConnectionId);
	RTP->Established = true;

	auto Resp               = xCreateRelayConnectionPairResp();
	Resp.ClientConnectionId = PP->ClientConnectionId;
	Resp.ConnectionPairId   = PP->ConnectionPairId;

	ubyte Buffer[MaxPacketSize];
	auto  RSize = WritePacket(Cmd_CreateConnectionResp, 0, Buffer, sizeof(Buffer), Resp);
	PostData(PP->ProxyConnectionId, Buffer, RSize);
}

void xTerminalRelay::OnTargetConnectionData(xRelayTerminalConnection * RTP, ubyte * DataPtr, size_t DataSize) {
	auto PP = GetConnectionPairById(RTP->RelayConnectionPairId);
	if (!PP) {
		X_DEBUG_PRINTF("Connection lost");
		return;
	}
	assert(PP->ProxyConnectionId);

	ubyte Buffer[MaxPacketSize];
	while (DataSize) {
		auto Req               = xRelayToProxyData();
		auto PostSize          = std::min(DataSize, MaxRelayPayloadSize);
		Req.ClientConnectionId = PP->ClientConnectionId;
		Req.DataView           = { (const char *)DataPtr, PostSize };

		auto RSize = WritePacket(Cmd_PostRelayToProxyData, 0, Buffer, sizeof(Buffer), Req);
		PostData(PP->ProxyConnectionId, Buffer, RSize);

		DataPtr  += PostSize;
		DataSize -= PostSize;
	}
}

void xTerminalRelay::OnTargetConnectionClosed(xRelayTerminalConnection * RTC) {
	auto PP = GetConnectionPairById(RTC->RelayConnectionPairId);
	if (!PP) {
		X_DEBUG_PRINTF("Connection lost");
		return;
	}
	assert(PP->ProxyConnectionId);

	X_DEBUG_PRINTF("IsEstablished=%s", YN(RTC->Established));
	if (RTC->Established) {
		auto Resp               = xCloseClientConnection();
		Resp.ClientConnectionId = PP->ClientConnectionId;
		ubyte Buffer[MaxPacketSize];
		auto  RSize = WritePacket(Cmd_CloseConnection, 0, Buffer, sizeof(Buffer), Resp);
		PostData(PP->ProxyConnectionId, Buffer, RSize);
	} else {
		auto Resp               = xCreateRelayConnectionPairResp();
		Resp.ClientConnectionId = PP->ClientConnectionId;
		Resp.ConnectionPairId   = 0;
		ubyte Buffer[MaxPacketSize];
		auto  RSize = WritePacket(Cmd_CreateConnectionResp, 0, Buffer, sizeof(Buffer), Resp);
		PostData(PP->ProxyConnectionId, Buffer, RSize);
	}
	KillConnection(RTC);
}

void xTerminalRelay::OnTargetUdpData(xRelayUdpChannel * UCP, void * DataPtr, size_t DataSize, const xNetAddress & RemoteAddress) {
	auto PP = GetConnectionPairById(UCP->RelayConnectionPairId);
	if (!PP) {
		X_DEBUG_PRINTF("Connection lost");
		return;
	}
	assert(PP->ProxyConnectionId);

	if (DataSize > MaxRelayPayloadSize) {
		X_DEBUG_PRINTF("Udp data too large to pass");
		return;
	}

	auto Req               = xRelayToProxyUdpData();
	Req.ClientConnectionId = PP->ClientConnectionId;
	Req.FromAddress        = RemoteAddress;
	Req.DataView           = { (const char *)DataPtr, DataSize };

	ubyte Buffer[MaxPacketSize];
	auto  RSize = WritePacket(Cmd_PostRelayToProxyUdpData, 0, Buffer, sizeof(Buffer), Req);
	PostData(PP->ProxyConnectionId, Buffer, RSize);
}

xRelayTerminalConnection * xTerminalRelay::GetTargetConnection(xRelayConnectionPair * RCP) {
	assert(IsTcp(RCP));
	return static_cast<xRelayTerminalConnection *>(RCP->UserCtx.P);
}

xRelayUdpChannel * xTerminalRelay::GetUdpAssociation(xRelayConnectionPair * RCP) {
	assert(IsUdp(RCP));
	return static_cast<xRelayUdpChannel *>(RCP->UserCtx.P);
}

void xTerminalRelay::SetTargetConnection(xRelayConnectionPair * RCP, xRelayTerminalConnection * RTC) {
	RCP->UserCtx.P = RTC;
}

void xTerminalRelay::SetUdpAssociation(xRelayConnectionPair * RCP, xRelayUdpChannel * RUC) {
	SetUdp(RCP);
	RCP->UserCtx.P = RUC;
}

void xTerminalRelay::KillConnectionPair(xRelayConnectionPair * RCPP) {
	auto ConnectinoPairId = RCPP->ConnectionPairId;
	if (IsUdp(RCPP)) {
		auto P = GetUdpAssociation(RCPP);
		Reset(P->RelayConnectionPairId);
		KillUdpAssociationList.GrabTail(*P);
	} else if (IsTcp(RCPP)) {
		auto P = GetTargetConnection(RCPP);
		Reset(P->RelayConnectionPairId);
		KillConnectionList.GrabTail(*P);
	}
	DestroyConnectionPair(ConnectinoPairId);
}

void xTerminalRelay::KillConnection(xRelayTerminalConnectionNode * NodePtr) {
	if (!NodePtr->RelayConnectionPairId) {  // already killed
		return;
	}
	DestroyConnectionPair(Steal(NodePtr->RelayConnectionPairId));
	KillConnectionList.GrabTail(*NodePtr);
}

void xTerminalRelay::KillUdpChannel(xRelayUdpChannelNode * NodePtr) {
	if (!NodePtr->RelayConnectionPairId) {  // already killed
		return;
	}
	DestroyConnectionPair(Steal(NodePtr->RelayConnectionPairId));
	KillUdpAssociationList.GrabTail(*NodePtr);
}
