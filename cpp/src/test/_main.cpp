#include "../common/base.hpp"
#include "../common_protocol/protocol_binary.hpp"

#include <core/core_min.hpp>
#include <iomanip>
#include <server_arch/client.hpp>

class xPBinary : public xBinaryMessage {
private:
	void SerializeMembers() override {

		W(A);
		W(B);
		W(C);
		W(D);

		W(S1);
		W(S2);
	}
	void DeserializeMembers() override {
		R(A);
		R(B);
		R(C);
		R(D);

		std::string RS1;
		std::string RS2;
		R(RS1);
		R(RS2);

		std::ios saved_state(nullptr);
		saved_state.copyfmt(std::cout);
		auto SG = xScopeGuard([&] { cout.copyfmt(saved_state); });

		cout << std::hex;
		cout << std::fixed;

		cout << "A: " << std::setw(16) << std::setfill('0') << A << endl;
		cout << "B: " << std::setw(16) << std::setfill('0') << B << endl;
		cout << "C: " << std::setw(16) << std::setfill('0') << C << endl;
		cout << "D: " << std::setw(16) << std::setfill('0') << D << endl;
		cout << "RS1: [" << RS1 << "]" << endl;
		cout << "RS2: [" << RS2 << "]" << endl;
	}

public:
	int8_t      A  = -0x01;
	int16_t     B  = -0x0102;
	int32_t     C  = -0x01020304;
	int64_t     D  = -0x0102030405060708;
	std::string S1 = "Hello world! 1";
	std::string S2 = "Hello world! 2";
};

int main(int argc, char ** argv) {

	xPBinary T;

	ubyte Buffer[64] = {};

	auto RSize = T.Serialize(Buffer, sizeof(Buffer));

	cout << "RSize: " << RSize << endl;
	cout << HexShow(Buffer, RSize) << endl;

	auto DR = T.Deserialize(Buffer, std::min((size_t)sizeof(Buffer), RSize));
	cout << "DR: " << DR << endl;

	return 0;
}
