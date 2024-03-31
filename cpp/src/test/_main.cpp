#include "../common/base.hpp"
#include "../component/terminal_service.hpp"

static auto IoCtx    = xIoContext();
static auto ICG      = xResourceGuard(IoCtx);
static auto RunState = xRunState();
static auto Ticker   = xTicker();

static const char * GET = "GET / HTTP/1.1\r\nHost: www.baidu.com\r\n\r\n";

class xRealTS : public xTerminalService {
public:
private:
	bool OnConnectionData(uint64_t ConnectionId, void * DataPtr, size_t DataSize) override {
		X_DEBUG_PRINTF("DataFrom:%" PRIx64 ", \nData:%s", ConnectionId, HexShow(DataPtr, DataSize).c_str());
		return true;
	}
};

static auto TS  = xRealTS();
static auto TSG = xResourceGuard(TS, &IoCtx);

int main(int argc, char ** argv) {
	RunState.Start();

	auto Op = xTerminalConnectionOptions{
		xNetAddress::Parse("183.2.172.42:80"),
		xNetAddress::Parse("192.168.5.106"),
	};
	auto CID = TS.CreateTerminalConnection(Op);
	X_DEBUG_PRINTF("CID: %" PRIx64 "", CID);
	TS.PostConnectionData(CID, GET, strlen(GET));

	while (true) {
		Ticker.Update();
		IoCtx.LoopOnce();
		TS.Tick(Ticker);
	}
	RunState.Finish();
	return 0;
}
