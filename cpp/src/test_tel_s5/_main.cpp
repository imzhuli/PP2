#include "../common/base.hpp"
#include "../common_job/job.hpp"

static auto IoCtx         = xIoContext();
static auto RunState      = xRunState();
static auto JQ            = xJobQueue();
static auto ProxyAddress  = xNetAddress::Parse("127.0.0.1:10010");
static auto User          = std::string("test_11");
static auto Password      = std::string("12345");
static auto TargetAddress = xNetAddress::Parse("127.0.0.1:9999");
static auto S5ReadyEvent  = xEvent();

struct PostJob : xJobNode {
	std::string Input;
};

struct TelS5Client : xTcpConnection::iListener {
	enum eS5State {
		S5_UNSPEC,
		S5_INIT,
		S5_AUTH,
		S5_CONNECT,
		S5_READY,
		S5_CLOSED,
	};

	bool Init() {
		if (!Conn.Init(&IoCtx, ProxyAddress, this)) {
			return false;
		}
		HasConn = true;
		State   = S5_INIT;
		return true;
	}

	void Clean() {
		if (Steal(HasConn)) {
			Conn.Clean();
		}
	}

	void OnConnected(xTcpConnection * TcpConnectionPtr) override {
		ubyte Buffer[3] = { 0x05, 0x01, 0x02 };
		TcpConnectionPtr->PostData(Buffer, sizeof(Buffer));
	}
	size_t OnData(xTcpConnection * TcpConnectionPtr, void * _, size_t DataSize) override {
		auto DataPtr = (ubyte *)_;
		if (State == S5_INIT) {
			if (DataSize < 2) {
				return 0;
			}
			cout << "Client greeting resp: " << endl;
			cout << HexShow(DataPtr, 2) << endl;
			if (DataPtr[0] != 0x05) {
				X_PERROR("Invalid server choice");
				quick_exit(-1);
			}
			if (DataPtr[1] != 0x02) {
				X_PERROR("No user/pass auth");
				quick_exit(-1);
			}

			ubyte Buffer[1024];
			auto  W = xStreamWriter(Buffer);
			W.W1(0x01);
			W.W1((uint8_t)User.size());
			W.W(User.data(), User.size());
			W.W1((uint8_t)Password.size());
			W.W(Password.data(), Password.size());
			TcpConnectionPtr->PostData(Buffer, W.Offset());
			State = S5_AUTH;
			return 2;
		} else if (State == S5_AUTH) {
			if (DataSize < 2) {
				return 0;
			}
			cout << "Client auth resp: " << endl;
			cout << HexShow(DataPtr, 2) << endl;

			if (DataPtr[0] != 0x01) {
				X_PERROR("Invalid auth response");
				quick_exit(-1);
			}
			if (DataPtr[1]) {
				X_PERROR("Authentication failed");
				quick_exit(-1);
			}
			cout << "Client auth succeeded" << endl;

			ubyte Buffer[1024];
			auto  W = xStreamWriter(Buffer);
			W.W1(0x05);  // VER
			W.W1(0x01);  // TCP_CONNECT
			W.W1(0x00);  // RESERVED
			RuntimeAssert((bool)TargetAddress);
			if (TargetAddress.IsV4()) {
				W.W1(0x01);
				W.W(TargetAddress.SA4, 4);
			} else {
				assert(TargetAddress.IsV6());
				W.W1(0x04);
				W.W(TargetAddress.SA6, 16);
			}
			W.W2(TargetAddress.Port);
			TcpConnectionPtr->PostData(Buffer, W.Offset());
			State = S5_CONNECT;
			return 2;
		} else if (State == S5_CONNECT) {
			size_t TotalRequiredSize = 3 + 1 + 2;  // ver+status+reserved + type + (addr) + port;
			if (DataSize < TotalRequiredSize) {
				return 0;
			}
			auto R = xStreamReader(DataPtr);
			if (R.R1() != 0x05 || R.R1() != 0 || R.R1() != 0) {
				X_PERROR("Failed to get connected");
				quick_exit(-1);
			}
			auto Type = R.R1();
			if (Type == 0x01) {  // ipv4
				TotalRequiredSize += 4;
			} else if (Type == 0x04) {  // ipv6
				TotalRequiredSize += 16;
			} else {
				X_PERROR("Unsupported server bind address");
			}
			if (DataSize < TotalRequiredSize) {
				return 0;
			}
			cout << "Connection established" << endl;
			State = S5_READY;
			S5ReadyEvent.Notify();
			return TotalRequiredSize;
		} else if (State == S5_READY) {
			cout << endl;
			cout << ">>>>>>>> DOWNSTREAM" << endl;
			cout << HexShow(DataPtr, DataSize) << endl;
			cout << "<<<<<<<< DOWNSTREAM END" << endl;
			return DataSize;
		}
		return DataSize;
	}
	void OnPeerClose(xTcpConnection * TcpConnectionPtr) override {
		State = S5_CLOSED;
		RunState.Stop();
		S5ReadyEvent.Notify();
	}

	void PostData(const void * DataPtr, size_t Data) {
		assert(HasConn);
		Conn.PostData(DataPtr, Data);
	}

	xTcpConnection Conn;
	bool           HasConn = false;
	eS5State       State   = S5_UNSPEC;
};

void IoThread() {
	auto ICG = xResourceGuard(IoCtx);
	RuntimeAssert(ICG);
	auto TC  = TelS5Client();
	auto TCG = xResourceGuard(TC);
	if (!TCG) {
		X_PERROR("Failed to init connection to proxy");
		RunState.Stop();
		S5ReadyEvent.Notify();
		return;
	}

	auto JL = xJobList();
	while (RunState) {
		IoCtx.LoopOnce();
		JQ.GrabJobList(JL);
		for (auto & N : JL) {
			auto   J     = static_cast<PostJob *>(&N);
			auto & Input = J->Input;
			TC.PostData(Input.data(), Input.size());
			delete J;
		}
	}
	JQ.GrabJobList(JL);
	for (auto & N : JL) {
		auto J = static_cast<PostJob *>(&N);
		cout << "Unprocessed job: " << J->Input << endl;
		delete J;
	}
}

int main(int argc, char ** argv) {
	RuntimeAssert(RunState.Start());
	auto T = std::thread(IoThread);
	S5ReadyEvent.Wait();
	while (RunState) {
		std::string Input;
		if (!std::getline(std::cin, Input)) {
			break;
		}
		auto J   = new PostJob();
		J->Input = Input;
		JQ.PostJob(*J);
	}
	RunState.Stop();
	T.join();
	RunState.Finish();
	return 0;
}
