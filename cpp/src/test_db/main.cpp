#include "../common/config.hpp"

#include <cinttypes>
#include <core/core_time.hpp>
#include <pp2db/pp2db.hpp>
#include <thread>

using namespace xel;
using namespace pp2db;

std::string DbUser;
std::string DbPass;
std::string DbName;
std::string DbHost;
uint16_t    DbPort;

void OnStaticIpResult(xVariable CC, const xResultBase * ResultPtr) {
	if (!ResultPtr) {
		X_PDEBUG("=============> RequestId: %" PRIx64 ", no result", CC.U64);
		return;
	}
	auto RP = static_cast<const xQueryStaticIpResult *>(ResultPtr);
	X_PDEBUG("=============> RequestId: %" PRIx64 ", proxy_ip=%s, enable_udp=%s", CC.U64, RP->ExportIp.c_str(), YN(RP->EnableUdp));
}

int main(int argc, char * argv[]) {
	auto C = xConfig("./test_assets/ex.ini");
	C.Require(DbUser, "DbUser");
	C.Require(DbPass, "DbPass");
	C.Require(DbName, "DbName");
	C.Require(DbHost, "DbHost");
	C.Require(DbPort, "DbPort");

	RuntimeAssert(InitPP2DB(DbUser.c_str(), DbPass.c_str(), DbName.c_str(), DbHost.c_str(), DbPort, 30));

	uint64_t RId = 0;
	xTimer   Timer;
	while (true) {
		PostQueryStaticIp(OnStaticIpResult, { .U64 = ++RId }, "aa", "aa");
		PoolPP2DBResults();
		if (Timer.TestAndTag(std::chrono::milliseconds(3000))) {
			break;
		}
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
	CleanPP2DB();
	return 0;
}
