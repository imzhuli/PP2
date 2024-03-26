#include "../../pb/pp2/network.pb.h"
#include "../common/base.hpp"
#include "../common_job/job.hpp"
#include "../common_protocol/protocol.hpp"
#include "./audit.hpp"
#include "./dns_cache.hpp"
#include "./dns_job.hpp"

/**************/
class xDnsService : public xClient {
public:
	bool Init(const xNetAddress & TargetAddress) {
		RuntimeAssert(IoCtx.Init());
		RuntimeAssert(xClient::Init(&IoCtx, TargetAddress));
		return true;
	}

	void Clean() {
		xClient::Clean();
		IoCtx.Clean();
	}

	void Wakeup() {
		IoCtx.Interrupt();
	}

	void LoopOnce() {
		IoCtx.LoopOnce();
	}
	using xClient::Tick;

	void OnServerConnected() override {
		X_DEBUG_PRINTF("");
		ubyte Buffer[MaxPacketSize];
		auto  RSize = xPacket::MakeRegisterDispatcherConsumer(Buffer, sizeof(Buffer), InterestedCommandIds.data(), InterestedCommandIds.size());
		PostData(Buffer, RSize);
	}

	bool OnPacket(const xPacketHeader & Header, ubyte * PayloadPtr, size_t PayloadSize) {
		X_DEBUG_PRINTF("");
		switch (Header.CommandId) {
			case Cmd_HostQuery:
				OnHostQuery(Header, PayloadPtr, PayloadSize);
				break;
			default:
				break;
		}
		return true;
	}

	void OnHostQuery(const xPacketHeader & Header, ubyte * PayloadPtr, size_t PayloadSize) {
		auto R = network::xHostQuery();
		if (!R.ParseFromArray(PayloadPtr, PayloadSize) || R.hostname().empty()) {
			X_DEBUG_PRINTF("Invalid request format");
			return;
		}
		PostDnsRequest(R.hostname(), { .U64 = Header.RequestId });
	}

	void OnHostQueryResult(xVariable Context, const xNetAddress & Address) {
		auto R = network::xHostQueryResp();
		R.set_ip_string(Address.IpToString());
		ubyte Buffer[MaxPacketSize];
		auto  RSize = PbWritePacket(Cmd_HostQueryResp, Context.U64, Buffer, sizeof(Buffer), R);
		PostData(Buffer, RSize);
	}

private:
	xIoContext                    IoCtx                = {};
	std::vector<xPacketCommandId> InterestedCommandIds = { Cmd_HostQuery };
};
static xDnsService DnsService;

void OnDnsWakeup(xVariable Ctx) {
	auto PS = (xDnsService *)Ctx.P;
	PS->Wakeup();
}

void OnDnsResult(xVariable Ctx, const xNetAddress & A4) {
	X_DEBUG_PRINTF("%" PRIx64 ":%s", Ctx.U64, A4.IpToString().c_str());
	DnsService.OnHostQueryResult(Ctx, A4);
}

static xRunState RunState;

int main(int argc, char ** argv) {
	auto RunStateGuard   = xScopeGuard([] { RunState.Start(); }, [] { RunState.Finish(); });
	auto DnsServiceGuard = xScopeGuard([] { DnsService.Init(xNetAddress::Parse("127.0.0.1:10001")); }, [] { DnsService.Clean(); });
	auto CacheGuard      = xScopeGuard([] { InitDnsCache(OnDnsWakeup, { .P = &DnsService }); }, CleanDnsCache);

	auto Ticker = xTicker();
	while (RunState) {
		Ticker.Update();
		DnsService.LoopOnce();
		DnsService.Tick(Ticker);
		DispatchResults(OnDnsResult);
		ShrinkDnsCache();
		OutputAudit();
	}

	return 0;
}
