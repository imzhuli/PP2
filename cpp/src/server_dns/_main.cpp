#include "../common/base.hpp"
#include "../common_job/job.hpp"
#include "../common_protocol/network.hpp"
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

	bool OnPacket(const xPacketHeader & Header, ubyte * PayloadPtr, size_t PayloadSize) override {
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
		auto R = xHostQueryReq();
		if (!R.Deserialize(PayloadPtr, PayloadSize) || R.Hostname.empty()) {
			X_DEBUG_PRINTF("Invalid request format");
			return;
		}
		PostDnsRequest(R.Hostname, { .U64 = Header.RequestId });
	}

	void OnHostQueryResult(xVariable Context, const xNetAddress & Address4, const xNetAddress & Address6) {
		auto R  = xHostQueryResp();
		R.Addr4 = Address4;
		R.Addr6 = Address6;
		ubyte Buffer[MaxPacketSize];
		auto  RSize = WritePacket(Cmd_HostQueryResp, Context.U64, Buffer, sizeof(Buffer), R);
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

void OnDnsResult(xVariable Ctx, const xNetAddress & A4, const xNetAddress & A6) {
	X_DEBUG_PRINTF("%" PRIx64 ": ipv4=%s, ipv6=%s", Ctx.U64, A4.IpToString().c_str(), A6.IpToString().c_str());
	DnsService.OnHostQueryResult(Ctx, A4, A6);
}

static xRunState RunState;

int main(int argc, char ** argv) {
	auto CL = xCommandLine(
		argc, argv,
		{
			{ 'c', nullptr, "dispatcher_comsumer_address", true },
		}
	);

	auto ConsumerAddressOpt = CL["dispatcher_comsumer_address"];
	if (!ConsumerAddressOpt()) {
		X_PERROR("missing option: -c dispatcher_comsumer_address");
		return -1;
	}

	auto RunStateGuard   = xScopeGuard([&] { RunState.Start(); }, [] { RunState.Finish(); });
	auto CacheGuard      = xScopeGuard([&] { InitDnsCache(OnDnsWakeup, { .P = &DnsService }); }, CleanDnsCache);
	auto DnsServiceGuard = xScopeGuard([&] { DnsService.Init(xNetAddress::Parse(*ConsumerAddressOpt)); }, [] { DnsService.Clean(); });

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
