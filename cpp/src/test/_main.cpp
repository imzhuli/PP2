#include "../common/base.hpp"
#include "../component/terminal_service.hpp"

static auto IoCtx    = xIoContext();
static auto ICG      = xResourceGuard(IoCtx);
static auto RunState = xRunState();
static auto Ticker   = xTicker();

static const char * GET      = "GET / HTTP/1.1\r\nHost: www.baidu.com\r\n\r\n";
static auto         TouchAll = xInstantRun([] { Touch(GET); });

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

struct xBM : xBinaryMessage {

	void SerializeMembers() override {
		W(EmptyString, I, Addr, J);
	}
	void DeserializeMembers() override {
		R(EmptyString, I, AddrView, J);
	}

	std::string EmptyString = "Something!!!";
	int         I           = 12345;
	xNetAddress Addr        = xNetAddress::Parse("192.168.123.1:8080");
	int         J           = 7890;

	std::string_view AddrView;
};

int main(int argc, char ** argv) {
	RunState.Start();

	ubyte Buffer[1024];

	xBM BM;
	cout << ", " << BM.EmptyString.length() << endl;
	auto WS = BM.Serialize(Buffer, sizeof(Buffer));
	cout << "B: " << HexShow(Buffer, WS) << endl;
	BM.EmptyString = "Hello world!";
	BM.I           = 0;
	BM.Addr        = xNetAddress();
	BM.J           = 0;
	BM.Deserialize(Buffer, sizeof(Buffer));

	cout << "BM.EmptyString: " << BM.EmptyString << ", " << BM.EmptyString.length() << endl;
	cout << "BM.I: " << BM.I << endl;
	cout << "BM.AddrView: " << string(BM.AddrView) << endl;
	cout << "BM.J: " << BM.J << endl;

	// auto Op = xTerminalConnectionOptions{
	// 	xNetAddress::Parse("183.2.172.42:80"),
	// 	xNetAddress::Parse("192.168.5.106"),
	// };
	// auto CID = TS.CreateTerminalConnection(Op);
	// X_DEBUG_PRINTF("CID: %" PRIx64 "", CID);
	// TS.PostConnectionData(CID, GET, strlen(GET));

	// while (true) {
	// 	Ticker.Update();
	// 	IoCtx.LoopOnce();
	// 	TS.Tick(Ticker);
	// }
	RunState.Finish();
	return 0;
}
