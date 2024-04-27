#include "../common/base.hpp"
#include "../common/config.hpp"
#include "../common_protocol/client_auth.hpp"
#include "../common_protocol/protocol.hpp"
#include "../component/static_ip_table.hpp"
#include "./address_binding.hpp"
#include "./request.hpp"

#include <pp2db/pp2db.hpp>
#include <unordered_map>

using namespace pp2db;

struct xExtractedAuthInfo {
	int32_t     AuditId;
	int32_t     PoolId;
	uint32_t    CountryId;
	uint32_t    CityId;
	xNetAddress StaticIp;
};

static xStaticIpTable IpTable;

static void OnStaticIpResult(xVariable CC, const xResultBase * ResultPtr);

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
		X_DEBUG_PRINTF("[%s:%s@%s]", Req.AccountName.c_str(), Req.Password.c_str(), Req.AddressString.c_str());
		PostQueryStaticIp(OnStaticIpResult, { .U64 = Header.RequestId }, Req.AccountName, Req.Password);
	}

	TerminalControllerBinding GetTerminalControllerBinding(const std::string & BindAddress) {
		auto Iter = IpTable.find(BindAddress);
		if (Iter == IpTable.end()) {
			return {};
		}
		auto & R = Iter->second;
		return {
			.TerminalControllerAddress = R.TerminalControllerAddress,
			.TerminalId                = R.TerminalId,
		};
	}

	std::vector<xPacketCommandId> InterestedCommandIds = { Cmd_ProxyClientAuth };

public:
	void OnDbAuthResult(uint64_t RequestContextId, const xQueryStaticIpResult * RP) {
		if (!RP) {
			X_DEBUG_PRINTF("=============> RequestId: %" PRIx64 ", no result", RequestContextId);
			return;
		}
		X_DEBUG_PRINTF("=============> RequestId: %" PRIx64 ", proxy_ip=%s, enable_udp=%s", RequestContextId, RP->ExportIp.c_str(), YN(RP->EnableUdp));

		auto Resp = xProxyClientAuthResp();

		if (!RP) {
		} else {
			auto Binding = GetTerminalControllerBinding(RP->ExportIp);
			if (Binding.TerminalControllerAddress) {
				Resp.AuditKey                  = -1;
				Resp.CacheTimeout              = 300;
				Resp.TerminalControllerAddress = Binding.TerminalControllerAddress;  // relay server, or terminal service address
				Resp.TerminalId                = Binding.TerminalId;                 // index in relay server
			}
		}

		ubyte Buffer[MaxPacketSize];
		auto  RSize = WritePacket(Cmd_ProxyClientAuthResp, RequestContextId, Buffer, sizeof(Buffer), Resp);
		if (!RSize) {
			return;
		}
		PostData(Buffer, RSize);
	}
};

std::string DbUser;
std::string DbPass;
std::string DbName;
std::string DbHost;
uint16_t    DbPort;

auto IoCtx               = xIoContext();
auto AuthService         = xAuthService();
auto RunState            = xRunState();
auto Ticker              = xTicker();
auto AuthConsumerAddress = xNetAddress();

void OnStaticIpResult(xVariable CC, const xResultBase * ResultPtr) {
	auto RequestId = CC.U64;
	auto RP        = static_cast<const xQueryStaticIpResult *>(ResultPtr);
	AuthService.OnDbAuthResult(RequestId, RP);
}

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
	auto IpTableFile = std::string();
	C.Require(IpTableFile, "ip_table_file");
	IpTable = LoadStaticIpTable(IpTableFile.c_str());

	C.Require(DbUser, "DbUser");
	C.Require(DbPass, "DbPass");
	C.Require(DbName, "DbName");
	C.Require(DbHost, "DbHost");
	C.Require(DbPort, "DbPort");
	RuntimeAssert(InitPP2DB(DbUser.c_str(), DbPass.c_str(), DbName.c_str(), DbHost.c_str(), DbPort, 30));

	auto RSG = xScopeGuard([] { RunState.Start(); }, [] { RunState.Finish(); });
	auto ICG = xResourceGuard(IoCtx);
	RuntimeAssert(ICG);
	auto ASG = xResourceGuard(AuthService, &IoCtx, AuthConsumerAddress);
	RuntimeAssert(ASG);

	while (RunState) {
		Ticker.Update();
		IoCtx.LoopOnce();
		PoolPP2DBResults();
		AuthService.Tick(Ticker);
	}

	CleanPP2DB();
	return 0;
}
