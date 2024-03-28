#include "../common/base.hpp"
#include "../common_protocol/challenge.hpp"

#include <core/core_min.hpp>
#include <iomanip>
#include <server_arch/client.hpp>

int main(int argc, char ** argv) {

	xChallenge C;

	ubyte Buffer[MaxPacketSize];
	auto  RSize = WritePacket(0x01, 0x02, Buffer, sizeof(Buffer), C);

	cout << "RSize: " << RSize << endl;
	cout << HexShow(Buffer, RSize) << endl;

	auto D = C.Deserialize(Buffer + PacketHeaderSize, RSize - PacketHeaderSize);
	cout << D << endl;
	cout << YN(D == RSize - PacketHeaderSize) << endl;
	cout << C.HW << endl;

	return 0;
}
