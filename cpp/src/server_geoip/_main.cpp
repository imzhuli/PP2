#include "../common/base.hpp"

#include <core/core_min.hpp>
#include <server_arch/client.hpp>

class xGeoIpConsumer : public xClient {
public:
	using xClient::Clean;
	using xClient::Init;

	bool Init(xIoContext * IoContextPtr, const xNetAddress & TargetAddress) {
		return xClient::Init(IoContextPtr, TargetAddress, xNetAddress::Parse("127.0.0.1"));
	}

	void OnServerConnected() override {
		X_DEBUG_PRINTF("");
		ubyte Buffer[MaxPacketSize];
		auto  RSize = xPacket::MakeRegisterDispatcherConsumer(Buffer, sizeof(Buffer), InterestedCommandIds.data(), InterestedCommandIds.size());
		PostData(Buffer, RSize);
	}

	std::vector<xPacketCommandId> InterestedCommandIds = { 0x01 };
};

auto IoCtx    = xIoContext();
auto Consumer = xGeoIpConsumer();

int main(int argc, char ** argv) {

	auto CL = xCommandLine(
		argc, argv,
		{
			{ 'd', nullptr, "dispatcher_consumer", true },
		}
	);

	auto OptD = CL["dispatcher_consumer"];
	if (!OptD()) {
		cerr << "invalid usage" << endl;
		return -1;
	}

	auto BAddress = xNetAddress::Parse(OptD->c_str());
	auto IG       = xResourceGuard(IoCtx);
	auto OG       = xResourceGuard(Consumer, &IoCtx, BAddress);

	auto T = xTicker();
	while (true) {
		T.Update();
		IoCtx.LoopOnce();
		Consumer.Tick(T);
	}
}
