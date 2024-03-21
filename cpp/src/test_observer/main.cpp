#include "../common/base.hpp"

#include <core/core_min.hpp>
#include <server_arch/client.hpp>

class xObserver : public xClient {
public:
	using xClient::Clean;
	using xClient::Init;

	void OnServerConnected() override {
		X_DEBUG_PRINTF("");
		ubyte Buffer[MaxPacketSize];
		auto  RSize = xPacket::MakeRegisterDispatcherObserver(Buffer, sizeof(Buffer), InterestedCommandIds.data(), InterestedCommandIds.size());
		PostData(Buffer, RSize);
	}

	std::vector<xPacketCommandId> InterestedCommandIds = { 0x01 };
};

auto IoCtx    = xIoContext();
auto Observer = xObserver();

int main(int argc, char ** argv) {

	auto CL = xCommandLine(
		argc, argv,
		{
			{ 'd', nullptr, "dispatcher_observer", true },
		}
	);

	auto OptD = CL["dispatcher_observer"];
	if (!OptD()) {
		cerr << "invalid usage" << endl;
		return -1;
	}
	auto BAddress = xNetAddress::Parse(OptD->c_str());
	auto IG       = xResourceGuard(IoCtx);
	auto OG       = xResourceGuard(Observer, &IoCtx, BAddress);

	auto T = xTicker();
	while (true) {
		T.Update();
		IoCtx.LoopOnce();
		Observer.Tick(T);
	}
}
