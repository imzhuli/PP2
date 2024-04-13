#include "../common/base.hpp"

static auto IC = xIoContext();
static auto TS = xTcpServer();

struct xHexShowService
	: xTcpServer::iListener
	, xTcpConnection::iListener {

	size_t OnData(xTcpConnection * TcpConnectionPtr, void * DataPtr, size_t DataSize) override {
		cout << "ConnectionData: " << TcpConnectionPtr << endl;
		cout << HexShow(DataPtr, DataSize) << endl;
		return DataSize;
	}

	void OnPeerClose(xTcpConnection * TcpConnectionPtr) override {
		cout << "ConnectionClose: " << TcpConnectionPtr << endl;
		TcpConnectionPtr->Clean();
		delete TcpConnectionPtr;
	}

	void OnNewConnection(xTcpServer * TcpServerPtr, xSocket && NativeHandle) override {
		assert(TcpServerPtr == &TS);
		auto CP = new (std::nothrow) xTcpConnection();
		if (!CP) {
			X_PERROR("Failed to allocate connection");
			return;
		}
		if (!CP->Init(&IC, std::move(NativeHandle), this)) {
			X_PERROR("Failed to init new connection");
			delete CP;
		}
		cout << "NewConnection: " << CP << endl;
	}
};
static auto HSS = xHexShowService();

int main(int argc, char ** argv) {

	auto CL = xCommandLine(
		argc, argv,
		{
			{ 'a', nullptr, "bind-address", true },
		}
	);
	auto BO = CL["bind-address"];
	if (!BO()) {
		cerr << "missing arg: -a bind-address" << endl;
		return -1;
	}
	auto ICG = xResourceGuard(IC);
	auto TSG = xResourceGuard(TS, &IC, xNetAddress::Parse(*BO), &HSS);
	RuntimeAssert(ICG);
	RuntimeAssert(TSG);
	while (true) {
		IC.LoopOnce();
	}

	return -1;
}
