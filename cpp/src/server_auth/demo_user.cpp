#include "./demo_user.hpp"

// 45.205.98.10

std::unordered_map<std::string, xDemoUser> DemoUsers;

auto Init = xInstantRun([] {
	DemoUsers.insert(std::make_pair(
		"test_5",
		xDemoUser{
			.AuditId           = 5,
			.ControllerAddress = xNetAddress::Parse("127.0.0.1:10020"),
			.TerminalId        = 5,
		}
	));
	DemoUsers.insert(std::make_pair(
		"test_6",
		xDemoUser{
			.AuditId           = 6,
			.ControllerAddress = xNetAddress::Parse("127.0.0.1:10020"),
			.TerminalId        = 6,
		}
	));
});
