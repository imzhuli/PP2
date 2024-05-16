#include "../common/base.hpp"
#include "../common_protocol/network.hpp"

#include <iostream>
using namespace std;

auto IC  = xIoContext();
auto ICG = xResourceGuard(IC);

static uint64_t RequestId = 1024;
struct xSender : xTcpConnection::iListener {
	void OnConnected(xTcpConnection * TcpConnectionPtr) {
		cout << "Connected" << endl;

		auto H1     = "www.qq.com";
		auto R1     = xHostQueryReq();
		R1.Hostname = H1;

		auto H2     = "www.baidu.com";
		auto R2     = xHostQueryReq();
		R2.Hostname = H2;

		auto H3     = "www.163.com";
		auto R3     = xHostQueryReq();
		R3.Hostname = H3;

		ubyte  Buffer[1024];
		size_t RZ = 0;

		RZ += WritePacket(Cmd_HostQuery, ++RequestId, Buffer + RZ, sizeof(Buffer) - RZ, R1);
		RZ += WritePacket(Cmd_HostQuery, ++RequestId, Buffer + RZ, sizeof(Buffer) - RZ, R2);
		RZ += WritePacket(Cmd_HostQuery, ++RequestId, Buffer + RZ, sizeof(Buffer) - RZ, R3);
		TcpConnectionPtr->PostData(Buffer, RZ);
	}
	void OnPeerClose(xTcpConnection * TcpConnectionPtr) {
		cout << "Peer close" << endl;
	}
	void OnFlush(xTcpConnection * TcpConnectionPtr) {
		cout << "flush" << endl;
	}
	size_t OnData(xTcpConnection * TcpConnectionPtr, void * DataPtr, size_t DataSize) {
		cout << "on data, size=" << DataSize << endl;
		cout << HexShow(DataPtr, DataSize) << endl;
		return DataSize;
	}
};

auto Conn   = xTcpConnection();
auto Sender = xSender();

auto TargetAddress = xNetAddress::Parse("127.0.0.1", 10000);

int main(int argc, char ** argv) {

	Conn.Init(&IC, TargetAddress, &Sender);

	auto T = xTimer();
	while (true) {
		IC.LoopOnce();
		if (T.TestAndTag(3s)) {
			break;
		}
	}
	return 0;
}
