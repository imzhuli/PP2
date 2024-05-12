#include "./proxy_access.hpp"

// xDispatcherClient
bool xProxyDispatcherClient::OnPacket(const xPacketHeader & Header, ubyte * PayloadPtr, size_t PayloadSize) {
	X_DEBUG_PRINTF("CmdId=%" PRIx64 "", Header.CommandId);
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
		default: {
			X_DEBUG_PRINTF("Unprocessed CmdId=%" PRIx64 "", Header.CommandId);
			break;
		}
	}
	return true;
}
