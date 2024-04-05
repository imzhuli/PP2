#include "../common/base.hpp"
#include "../common_protocol/protocol.hpp"
#include "../common_protocol/terminal.hpp"

class xDummyRelay : public xService {
public:
	bool OnPacket(xServiceClientConnection & Connection, const xPacketHeader & Header, ubyte * PayloadPtr, size_t PayloadSize) {
		switch (Header.CommandId) {
			case Cmd_CreateConnection:
				return OnCreateConnection(Connection, Header, PayloadPtr, PayloadSize);
			default:
				break;
		}
		return false;
	}
	bool OnCreateConnection(xServiceClientConnection & Connection, const xPacketHeader & Header, ubyte * PayloadPtr, size_t PayloadSize) {
		auto Req = xCreateTerminalConnection();
		if (!Req.Deserialize(PayloadPtr, PayloadSize)) {
			X_PERROR("Invalid protocol");
			return false;
		}
		auto Resp                       = xCreateTerminalConnectionResp();
		Resp.SourceConnectionId         = Req.SourceConnectionId;
		Resp.TerminalTargetConnectionId = 0x12345;
		ubyte Buffer[MaxPacketSize];
		auto  RSize = WritePacket(Cmd_CreateConnectionResp, Header.RequestId, Buffer, sizeof(Buffer), Resp);
		Connection.PostData(Buffer, RSize);
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
	}

	return 0;
}
