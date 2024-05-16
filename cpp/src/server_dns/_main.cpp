#include "../component/dns_service.hpp"
#include "./dns_server.hpp"

#include <iostream>

using namespace xel;
using namespace std;

static xDnsServer DnsServer;

std::vector<std::string> Hostnames = { "www.qq.com", "www.baidu.com", "zhuli.cool" };

void Test(xVariable Ctx, const xNetAddress & A4, const xNetAddress & A6) {
	auto   Index    = Ctx.I32;
	auto & Hostname = Hostnames[Index];
	cout << Hostname << ":" << A4.ToString() << endl;
}

int main(int argc, char ** argv) {

	auto CL = xCommandLine(
		argc, argv,
		{
			{ 'c', nullptr, "dispatcher_consumer_address", true },
		}
	);
	auto DCA = CL["dispatcher_consumer_address"];
	if (!DCA()) {
		cerr << "missing dispatcher_consumer_address" << endl;
		return -1;
	}

	auto DSG = xResourceGuard(DnsServer, xNetAddress::Parse(*DCA), 10000);

	while (true) {
		DnsServer.Tick();
	}

	return 0;
}
