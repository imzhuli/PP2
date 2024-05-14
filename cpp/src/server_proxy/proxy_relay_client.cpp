#include "./proxy_relay_client.hpp"

#include "./global.hpp"
#include "./proxy_access.hpp"

// xProxyRelayClient
bool xProxyRelayClient::Init(xProxyService * ProxyServicePtr, const xNetAddress & TargetAddress) {
	X_DEBUG_PRINTF("New connection to relay: %s", TargetAddress.ToString().c_str());
	if (!xTcpConnection::Init(ProxyServicePtr->IoCtxPtr, TargetAddress, this)) {
		return false;
	}
	this->ProxyServicePtr = ProxyServicePtr;
	this->TargetAddress   = TargetAddress;
	ResizeSendBuffer(1'500'000);
	ResizeRecvBuffer(1'500'000);
	return true;
}

void xProxyRelayClient::Clean() {
	xTcpConnection::Clean();
	Reset(ProxyServicePtr);
	Reset(TargetAddress);
}

void xProxyRelayClient::OnConnected(xTcpConnection * TcpConnectionPtr) {
	X_DEBUG_PRINTF("OnProxyRelayClient connected");
	Profiler.Reset();
	Profiler.MarkStartConnection();
	Profiler.MarkEstablished();
	PostRequestKeepAlive();
}

void xProxyRelayClient::OnPeerClose(xTcpConnection * TcpConnectionPtr) {
	Profiler.MarkCloseConnection();
	ProfilerLogger.I("ProxyRelayClient: %s", Profiler.Dump().ToString().c_str());
	ProxyServicePtr->RemoveRelayClient(this);
}

void xProxyRelayClient::PostData(const void * _, size_t DataSize) {
	Profiler.MarkUpload(DataSize);
	return xTcpConnection::PostData(_, DataSize);
}

size_t xProxyRelayClient::OnData(xTcpConnection * TcpConnectionPtr, void * DataPtrInput, size_t DataSize) {
	assert(TcpConnectionPtr == this);
	auto   DataPtr    = static_cast<ubyte *>(DataPtrInput);
	size_t RemainSize = DataSize;
	while (RemainSize >= PacketHeaderSize) {
		auto Header = xPacketHeader::Parse(DataPtr);
		if (!Header) { /* header error */
			X_PERROR("HeaderError: \n%s", HexShow(DataPtr, DataSize).c_str());
			return InvalidDataSize;
		}
		auto PacketSize = Header.PacketSize;  // make a copy, so Header can be reused
		if (RemainSize < PacketSize) {        // wait for data
			break;
		}
		auto PayloadPtr  = xPacket::GetPayloadPtr(DataPtr);
		auto PayloadSize = Header.GetPayloadSize();
		if (!OnPacket(Header, PayloadPtr, PayloadSize)) { /* packet error */
			return InvalidDataSize;
		}
		DataPtr    += PacketSize;
		RemainSize -= PacketSize;
	}
	auto ProcessedData = DataSize - RemainSize;
	Profiler.MarkDownload(ProcessedData);
	return ProcessedData;
}

void xProxyRelayClient::OnFlush(xTcpConnection * TcpConnectionPtr) {
	X_DEBUG_PRINTF("");
}

bool xProxyRelayClient::OnPacket(const xPacketHeader & Header, ubyte * PayloadPtr, size_t PayloadSize) {
	switch (Header.CommandId) {
		case Cmd_CreateConnectionResp: {
			auto Resp = xCreateRelayConnectionPairResp();
			if (!Resp.Deserialize(PayloadPtr, PayloadSize)) {
				X_DEBUG_PRINTF("Invalid protocol");
				break;
			}
			ProxyServicePtr->OnTerminalConnectionResult(Resp);
			break;
		}
		case Cmd_CreateUdpAssociationResp: {
			auto Resp = xCreateUdpAssociationResp();
			if (!Resp.Deserialize(PayloadPtr, PayloadSize)) {
				X_DEBUG_PRINTF("Invalid protocol");
				break;
			}
			ProxyServicePtr->OnTerminalUdpAssociationResult(Resp);
			break;
		}
		case Cmd_PostRelayToProxyData: {
			auto Post = xRelayToProxyData();
			if (!Post.Deserialize(PayloadPtr, PayloadSize)) {
				X_DEBUG_PRINTF("Invalid protocol");
				break;
			}
			ProxyServicePtr->OnRelayData(Post);
			break;
		}
		case Cmd_CloseConnection: {
			auto Post = xCloseClientConnection();
			if (!Post.Deserialize(PayloadPtr, PayloadSize)) {
				X_DEBUG_PRINTF("Invalid protocol");
				break;
			}
			ProxyServicePtr->OnCloseConnection(Post);
			break;
		}
		case Cmd_PostRelayToProxyUdpData: {
			auto Post = xRelayToProxyUdpData();
			if (!Post.Deserialize(PayloadPtr, PayloadSize)) {
				X_DEBUG_PRINTF("Invalid protocol");
				break;
			}
			ProxyServicePtr->OnRelayUdpData(Post);
			break;
		}
		default: {
			X_DEBUG_PRINTF("CommandId: %" PRIu32 ", RequestId:%" PRIx64 "", Header.CommandId, Header.RequestId);
			X_DEBUG_PRINTF("Unsupported packet");
		}
	}
	return true;
}
