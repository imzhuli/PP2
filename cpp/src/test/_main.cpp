#include "../common/base.hpp"
#include "../common_protocol/protocol.hpp"

#include <core/core_min.hpp>
#include <crypto/base64.hpp>
#include <server_arch/client.hpp>

class xObserver : public xClient {
public:
	using xClient::Clean;
	using xClient::Init;

	void OnServerConnected() override {
		X_DEBUG_PRINTF("");
		ubyte Buffer[MaxPacketSize];
		auto  W          = xStreamWriter(Buffer);
		auto  Header     = xPacketHeader();
		Header.CommandId = 0x01;
		Header.RequestId = 0x02;
		W.Skip(PacketHeaderSize);
		W.W("Hello world!", 12);
		Header.PacketSize = W.Offset();
		Header.Serialize(Buffer);

		PostData(Buffer, Header.PacketSize);
	}

	std::vector<xPacketCommandId> InterestedCommandIds = { 0x01 };
};

auto IoCtx    = xIoContext();
auto Observer = xObserver();

int main(int argc, char ** argv) {

	auto S = std::string("1234567890");
	auto E = Base64Encode(S.data(), S.length());
	cout << "E:" << E << endl;
	auto D = Base64Decode(E.data(), E.length());
	cout << "D:" << D << endl;

	auto CL = xCommandLine(
		argc, argv,
		{
			{ 'd', nullptr, "dispatcher_producer", true },
		}
	);

	auto OptD = CL["dispatcher_producer"];
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
