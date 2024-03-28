#include "../common/base.hpp"
#include "../common_protocol/protocol_binary.hpp"

#include <core/core_min.hpp>
#include <iomanip>
#include <server_arch/client.hpp>

class xPBinary : public xBinaryMessage {
private:
	void InternalSerialize() override {
		auto S = std::string("Hello world!");
		W1(0x01);
		W2(0x0102);
		W4(0x01020304);
		W8(0x0102030405060708);
		WB(S.data(), S.size());
	}
	void InternalDeserialize() override {
		auto A = R1();
		auto B = R2();
		auto C = R4();
		auto D = R8();
		auto E = RB();

		std::ios saved_state(nullptr);
		saved_state.copyfmt(std::cout);
		auto SG = xScopeGuard([&] { cout.copyfmt(saved_state); });

		cout << std::hex;
		cout << std::fixed;

		cout << "A: " << std::setw(16) << std::setfill('0') << (unsigned int)A << endl;
		cout << "B: " << std::setw(16) << std::setfill('0') << B << endl;
		cout << "C: " << std::setw(16) << std::setfill('0') << C << endl;
		cout << "D: " << std::setw(16) << std::setfill('0') << D << endl;
		cout << "E: [" << E << "]" << endl;
	}
};

int main(int argc, char ** argv) {

	xPBinary T;

	ubyte Buffer[64];
	auto  RSize = T.Serialize(Buffer, 32);

	cout << "RSize: " << RSize << endl;
	cout << HexShow(Buffer, RSize) << endl;

	auto DR = T.Deserialize(Buffer, std::min((size_t)sizeof(Buffer), RSize));
	cout << "DR: " << DR << endl;

	cout << 123 << endl;
	return 0;
}
