#include "../common/base.hpp"

#include <iostream>
using namespace std;

auto IC  = xIoContext();
auto ICG = xResourceGuard(IC);

struct xSender : xTcpConnection::iListener {
	void OnConnected(xTcpConnection * TcpConnectionPtr) {
		cout << "Connected" << endl;
	}
	void OnPeerClose(xTcpConnection * TcpConnectionPtr) {
		cout << "Peer close" << endl;
	}
	void OnFlush(xTcpConnection * TcpConnectionPtr) {
		cout << "flush" << endl;
	}
	size_t OnData(xTcpConnection * TcpConnectionPtr, void * DataPtr, size_t DataSize) {
		cout << "on data, size=" << DataSize << endl;
		return DataSize;
	}
};

auto Conn   = xTcpConnection();
auto Sender = xSender();

auto         TargetAddress          = xNetAddress::Parse("127.0.0.1", 12345);
static ubyte DataBuffer[10'096'000] = {};

int main(int argc, char ** argv) {

	Conn.Init(&IC, TargetAddress, &Sender);

	auto T = xTimer();
	while (true) {
		IC.LoopOnce();
		if (T.TestAndTag(1s)) {
			Conn.PostData(DataBuffer, sizeof(DataBuffer));
		}
	}

	return 0;
}
