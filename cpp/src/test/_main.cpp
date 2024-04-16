#include "../common/base.hpp"
#include "../component/terminal_service.hpp"

#include <network/udp_channel.hpp>

static auto IoCtx = xIoContext();
static auto ICG   = xResourceGuard(IoCtx);

static auto RunState = xRunState();
static auto Ticker   = xTicker();

static const char * GET      = "GET / HTTP/1.1\r\nHost: www.baidu.com\r\n\r\n";
static auto         TouchAll = xInstantRun([] { Touch(RunState, Ticker, GET); });

struct xUdpTest : xUdpChannel::iListener {
	void OnData(xUdpChannel * ChannelPtr, void * DataPtr, size_t DataSize, const xNetAddress & RemoteAddress) {
		X_DEBUG_PRINTF("UdpData: @\n%s", RemoteAddress.ToString().c_str(), HexShow(DataPtr, DataSize).c_str());
	}
};

int main(int argc, char ** argv) {

	auto CL = xCommandLine(
		argc, argv,
		{
			{ 'a', nullptr, "address", true },
		}
	);

	auto TAO = CL["address"];
	if (!TAO()) {
		cerr << "miss -a target_address" << endl;
		return 1;
	}
	auto TA = xNetAddress::Parse(*TAO);

	RunState.Start();

	auto UT = xUdpTest();
	auto U  = xUdpChannel();
	U.Init(&IoCtx, TA.Decay(), &UT);
	U.PostData(GET, strlen(GET), TA);

	while (true) {
		IoCtx.LoopOnce(1024);
	}

	RunState.Finish();
	return 0;
}
