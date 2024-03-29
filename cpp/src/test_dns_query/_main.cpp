#include "../../pb/pp2/network.pb.h"
#include "../common/base.hpp"
#include "../common_protocol/protocol_buffers.hpp"

#include <core/core_min.hpp>
#include <server_arch/client.hpp>

class xTcpTester : public xClient {
public:
	void OnServerConnected() override {
		auto R       = network::xHostQueryReq();
		auto AddrStr = "www.baidu.com";

		R.set_hostname(AddrStr);

		ubyte  Buffer[MaxPacketSize];
		size_t RSize = PbWritePacket(Cmd_HostQuery, 1, Buffer, sizeof(Buffer), R);

		cout << HexShow(Buffer, RSize) << endl;
		PostData(Buffer, RSize);
	}

	bool OnPacket(const xPacketHeader & Header, ubyte * PayloadPtr, size_t PayloadSize) override {
		printf("OnPacket: Cmd=%" PRIx32 ", ReqId=%" PRIx64 "\n%s\n", Header.CommandId, Header.RequestId, HexShow(PayloadPtr, PayloadSize).c_str());
		return true;
	}
};

int main(int argc, char ** argv) {

	auto IoCtx = xIoContext();
	auto ICG   = xResourceGuard(IoCtx);

	auto Tester = xTcpTester();
	auto TG     = xResourceGuard(Tester, &IoCtx, xNetAddress::Parse("127.0.0.1:10000"));

	auto T      = xTimer();
	auto Ticker = xTicker();
	while (!T.TestAndTag(std::chrono::seconds(2))) {
		IoCtx.LoopOnce();
		Tester.Tick(Ticker);
	}

	return 0;
}
