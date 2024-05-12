#include "../common/base.hpp"

#include <core/core_min.hpp>
#include <server_arch/client.hpp>

struct xCollector : public xTcpConnection::iListener {

	void Update() {
		Ticker.Update();
	}

	size_t OnData(xTcpConnection * TcpConnectionPtr, void * DataInputPtr, size_t DataSize) {
		auto DataPtr = (const char *)DataInputPtr;
		for (size_t i = 0; i < DataSize; ++i) {
			OnByte(DataPtr[i]);
		}
		return DataSize;
	}

	void OnByte(ubyte B) {
		LastHitTimestamp = Ticker;
	}

	xTicker  Ticker;
	uint64_t LastHitTimestamp;
};

auto IoCtx = xIoContext();
auto Col   = xCollector();

int main(int argc, char ** argv) {

	auto CL = xCommandLine(
		argc, argv,
		{
			{ 'a', nullptr, "target_address", true },
			{ 'b', nullptr, "bind_address", true },
		}
	);

	auto OptT = CL["target_address"];
	if (!OptT()) {
		cerr << "missing target address" << endl;
		return -1;
	}

	auto BAddress = xNetAddress();
	auto OptB     = CL["bind_address"];
	if (OptB()) {
		BAddress = xNetAddress::Parse(OptB->c_str());
	}

	auto TAddress = xNetAddress::Parse(OptT->c_str());
	auto IG       = xResourceGuard(IoCtx);

	Touch(TAddress);

	while (true) {
		Col.Update();
		IoCtx.LoopOnce();
	}
	return 0;
}
