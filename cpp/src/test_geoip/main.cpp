#include "../../pb/pp2/geo.pb.h"
#include "../common/base.hpp"
#include "../common_protocol/protocol_buffers.hpp"

#include <core/core_min.hpp>
#include <server_arch/client.hpp>

using namespace std;
using namespace xel;

class xTester : public xUdpChannel::iListener {
public:
	bool Init(const xNetAddress & TargetAddress) {
		if (!IoCtx.Init()) {
			return false;
		}
		auto IOC = MakeResourceCleaner(IoCtx);

		if (!Channel.Init(&IoCtx, AF_INET, this)) {
			return false;
		}
		auto CC = MakeResourceCleaner(Channel);

		this->TargetAddress = TargetAddress;

		Dismiss(IOC, CC);
		return true;
	}

	void Clean() {
		auto RCS = MakeResourceCleaner(IoCtx);
	}

	xIoContext * GetIoCtx() {
		return &IoCtx;
	}

	bool Sent = false;
	void Tick() {
		Ticker.Update();
		IoCtx.LoopOnce();
		if (!Steal(Sent, true)) {
			auto R = geo::xGeoInfoReq();
			R.set_ip("14.153.209.68");
			R.set_hello_world("Hello world!");

			ubyte  Buffer[MaxPacketSize];
			size_t RSize = PbWritePacket(Cmd_GeoQuery, 1, Buffer, sizeof(Buffer), R);

			cout << "Post: " << TargetAddress.ToString() << endl;
			cout << HexShow(Buffer, RSize) << endl;
			Channel.PostData(Buffer, RSize, TargetAddress);
		}
	}
	uint64_t GetTimestamp() {
		return Ticker;
	}

protected:
	void OnData(xUdpChannel * ChannelPtr, void * DataPtr, size_t DataSize, const xNetAddress & RemoteAddress) {
		cout << "UdpFrom: " << RemoteAddress.ToString() << endl;
		cout << HexShow(DataPtr, DataSize) << endl;
	}

private:
	xIoContext  IoCtx   = {};
	xUdpChannel Channel = {};
	xTicker     Ticker  = {};

	xNetAddress TargetAddress = {};
};

class xTcpTester : public xClient {
public:
	void OnServerConnected() {
		auto R = geo::xGeoInfoReq();
		R.set_ip("14.153.209.68");
		R.set_hello_world("Hello world!");

		ubyte  Buffer[MaxPacketSize];
		size_t RSize = PbWritePacket(Cmd_GeoQuery, 1, Buffer, sizeof(Buffer), R);

		cout << HexShow(Buffer, RSize) << endl;
		PostData(Buffer, RSize);
	}
};

auto R       = xRunState();
auto Tester  = xTester();
auto TTester = xTcpTester();

int main(int argc, char ** argv) {

	auto CL = xCommandLine(
		argc, argv,
		{
			{ 'u', nullptr, "udp_server", true },
			{ 'd', nullptr, "dispatcher_producer", true },
		}
	);

	auto OptD = CL["dispatcher_producer"];
	RuntimeAssert(OptD());

	auto OptU = CL["udp_server"];
	RuntimeAssert(OptU());

	auto TG  = xResourceGuard(Tester, xNetAddress::Parse(*OptU));
	auto TTG = xResourceGuard(TTester, Tester.GetIoCtx(), xNetAddress::Parse(*OptD));

	R.Start();
	while (R) {
		Tester.Tick();
		TTester.Tick(Tester.GetTimestamp());
	}
	R.Finish();
}
