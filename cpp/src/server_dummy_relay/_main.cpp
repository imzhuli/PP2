#include "../common/base.hpp"
#include "../common_protocol/data_exchange.hpp"
#include "../common_protocol/protocol.hpp"
#include "../component/terminal_controller_service.hpp"

class xDummyRelay : public xTerminalController {
public:
	bool OnPacket(xServiceClientConnection & Connection, const xPacketHeader & Header, ubyte * PayloadPtr, size_t PayloadSize) {
		switch (Header.CommandId) {
			case Cmd_CreateConnection:
				return OnCreateConnection(Connection, Header, PayloadPtr, PayloadSize);
			case Cmd_CloseConnection:
				return OnCloseConnection(Connection, Header, PayloadPtr, PayloadSize);
			case Cmd_PostProxyToRelayData:
				return OnProxyToRelay(Connection, Header, PayloadPtr, PayloadSize);
			default:
				X_DEBUG_PRINTF("OnPacket: %" PRIx32 ", rid=%" PRIx64 "\n%s", Header.CommandId, Header.RequestId, HexShow(PayloadPtr, PayloadSize).c_str());
				break;
		}
		return false;
	}
	bool OnCreateConnection(xServiceClientConnection & Connection, const xPacketHeader & Header, ubyte * PayloadPtr, size_t PayloadSize) {
		auto Req = xCreateRelayConnectionPair();
		if (!Req.Deserialize(PayloadPtr, PayloadSize)) {
			X_PERROR("Invalid protocol");
			return false;
		}
		auto Resp               = xCreateRelayConnectionPairResp();
		Resp.ClientConnectionId = Req.ClientConnectionId;

		auto PP = CreateConnectionPair();
		if (PP) {
			Resp.ConnectionPairId = PP->ConnectionPairId;
		}

		ubyte Buffer[MaxPacketSize];
		auto  RSize = WritePacket(Cmd_CreateConnectionResp, Header.RequestId, Buffer, sizeof(Buffer), Resp);
		Connection.PostData(Buffer, RSize);
		return true;
	}
	bool OnCloseConnection(xServiceClientConnection & Connection, const xPacketHeader & Header, ubyte * PayloadPtr, size_t PayloadSize) {
		auto Req = xCloseRelayConnectionPair();
		if (!Req.Deserialize(PayloadPtr, PayloadSize)) {
			X_PERROR("Invalid protocol");
			return false;
		}
		auto PP = GetConnectionPairById(Req.ConnectionPairId);
		if (!PP) {
			return true;
		}
		// TODO: post close connection to terminal

		//
		X_DEBUG_PRINTF("OnCloseConnection: ConnectionPairId=%" PRIx64 ", Found=%s", Req.ConnectionPairId, YN(PP));
		DestroyConnectionPair(PP);
		return true;
	}

	bool OnProxyToRelay(xServiceClientConnection & Connection, const xPacketHeader & Header, ubyte * PayloadPtr, size_t PayloadSize) {
		auto Req = xProxyToRelayData();
		if (!Req.Deserialize(PayloadPtr, PayloadSize)) {
			return true;
		}
		X_DEBUG_PRINTF("ProxyToRelay: \n%s", HexShow(Req.DataView.data(), Req.DataView.size()).c_str());
		return true;
	}
};

static auto IoCtx    = xIoContext();
static auto RunState = xRunState();
static auto Dummy    = xDummyRelay();

int main(int argc, char ** argv) {
	auto CL = xCommandLine(
		argc, argv,
		{
			{ 'a', nullptr, "bind_address", true },
		}
	);

	auto BindAddressOpt = CL["bind_address"];
	if (!BindAddressOpt()) {
		X_PERROR("missing option: -a bind_address");
		return -1;
	}
	auto BindAddress = xNetAddress::Parse(*BindAddressOpt);

	auto ICG = xResourceGuard(IoCtx);
	RuntimeAssert(ICG);
	auto DRG = xResourceGuard(Dummy, &IoCtx, BindAddress, true);
	RuntimeAssert(DRG);
	auto RunStateGuard = xScopeGuard([&] { RunState.Start(); }, [] { RunState.Finish(); });

	auto Ticker = xTicker();
	while (RunState) {
		Ticker.Update();
		IoCtx.LoopOnce();
		Dummy.Tick(Ticker);
	}

	return 0;
}
