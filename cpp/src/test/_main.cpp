#include "../common/base.hpp"
#include "../component/terminal_service.hpp"

static auto IoCtx    = xIoContext();
static auto ICG      = xResourceGuard(IoCtx);
static auto RunState = xRunState();
static auto Ticker   = xTicker();

class xRealTS : public xTerminalService {
public:
private:
	void OnConnectionEstablished(uint64_t ConnectionId) {
		X_DEBUG_PRINTF("");
	}
	void OnConnectionData(uint64_t ConnectionId, void * DataPtr, size_t DataSize) override {
		X_DEBUG_PRINTF("DataFrom:%" PRIx64 ", \nData:%s", HexShow(DataPtr, DataSize).c_str());
	}
};

static auto TS  = xRealTS();
static auto TSG = xResourceGuard(TS, &IoCtx);

int main(int argc, char ** argv) {
	RunState.Start();

	auto Op = xTerminalConnectionOptions{
		xNetAddress::Parse("183.2.172.42"),
		xNetAddress::Parse("192.168.5.105"),
	};
	auto CID = TS.CreateTerminalConnection(Op);
	X_DEBUG_PRINTF("CID: %" PRIx64 "", CID);

	while (true) {
		Ticker.Update();
		IoCtx.LoopOnce();
		TS.Tick(Ticker);
	}
	RunState.Finish();
	return 0;
}
