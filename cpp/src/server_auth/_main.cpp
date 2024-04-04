#include "../common/base.hpp"
#include "../common/config.hpp"
#include "../common_protocol/client_auth.hpp"
#include "../common_protocol/protocol.hpp"

#include <unordered_map>

struct xExtractedAuthInfo {
	int32_t     AuditId;
	int32_t     PoolId;
	uint32_t    CountryId;
	uint32_t    CityId;
	xNetAddress StaticIp;
};

std::unordered_map<std::string, xExtractedAuthInfo> AuthMap;

class xAuthService : public xClient {
protected:
	void OnServerConnected() override {
		X_DEBUG_PRINTF("");
		ubyte Buffer[MaxPacketSize];
		auto  RSize = xPacket::MakeRegisterDispatcherConsumer(Buffer, sizeof(Buffer), InterestedCommandIds.data(), InterestedCommandIds.size());
		PostData(Buffer, RSize);
	}

	bool OnPacket(const xPacketHeader & Header, ubyte * PayloadPtr, size_t PayloadSize) override {
		switch (Header.CommandId) {
			case Cmd_ProxyClientAuth: {
				OnClientAuth(Header, PayloadPtr, PayloadSize);
				break;
			}
			default:
				X_DEBUG_PRINTF("Unknown command id: %" PRIx64 "", Header.CommandId);
				break;
		}
		return true;
	}

	void OnClientAuth(const xPacketHeader & Header, ubyte * PayloadPtr, size_t PayloadSize) {
		auto Req = xProxyClientAuth();
		if (!Req.Deserialize(PayloadPtr, PayloadSize)) {
			X_PERROR("Invalid protocol");
			return;
		}
		X_DEBUG_PRINTF("ClientAuth: [%s:%s@%s]", Req.AccountName.c_str(), Req.Password.c_str(), Req.AddressString.c_str());

		auto Resp = xProxyClientAuthResp();
		// TODO: use real data
		Resp.AuditKey                   = 1;
		Resp.CacheTimeout               = 300;
		Resp.TerminalControllerAddress  = xNetAddress::Parse("192.168.123.45:6789");  // relay server, or terminal service address
		Resp.TerminalControllerSubIndex = 1024;                                       // index in relay server
		ubyte Buffer[MaxPacketSize];
		auto  RSize = WritePacket(Cmd_ProxyClientAuthResp, Header.RequestId, Buffer, sizeof(Buffer), Resp);
		if (!RSize) {
			return;
		}
		PostData(Buffer, RSize);
	}

	std::vector<xPacketCommandId> InterestedCommandIds = { Cmd_ProxyClientAuth };
};

auto IoCtx               = xIoContext();
auto AuthService         = xAuthService();
auto RunState            = xRunState();
auto Ticker              = xTicker();
auto AuthConsumerAddress = xNetAddress();

int main(int argc, char ** argv) {

	auto CL = xCommandLine(
		argc, argv,
		{
			{ 'c', nullptr, "config", true },
		}
	);
	auto ConfigOpt = CL["config"];
	if (!ConfigOpt()) {
		cerr << "missing -c config_filename" << endl;
		return -1;
	}
	auto C = xConfig(ConfigOpt->c_str());
	C.Require(AuthConsumerAddress, "auth_consumer_address");

	auto RSG = xScopeGuard([] { RunState.Start(); }, [] { RunState.Finish(); });
	auto ICG = xResourceGuard(IoCtx);
	RuntimeAssert(ICG);
	auto ASG = xResourceGuard(AuthService, &IoCtx, AuthConsumerAddress);
	RuntimeAssert(ASG);

	while (RunState) {
		Ticker.Update();
		IoCtx.LoopOnce();
		AuthService.Tick(Ticker);
	}

	return 0;
}
