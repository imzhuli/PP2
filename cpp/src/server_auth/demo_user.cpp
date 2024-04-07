#include "./demo_user.hpp"

// 45.205.98.10

std::unordered_map<std::string, xDemoUser> DemoUsers;

auto Init = xInstantRun([] {
	DemoUsers.insert(std::make_pair(
		"test_10",
		xDemoUser{
			.ControllerAddress = xNetAddress::Parse("127.0.0.1:10020"),
			.AuditId           = 10,
			.TerminalId        = 10,
		}
	));
	DemoUsers.insert(std::make_pair(
		"test_11",
		xDemoUser{
			.ControllerAddress = xNetAddress::Parse("127.0.0.1:10020"),
			.AuditId           = 11,
			.TerminalId        = 11,
		}
	));
	DemoUsers.insert(std::make_pair(
		"test_12",
		xDemoUser{
			.ControllerAddress = xNetAddress::Parse("127.0.0.1:10020"),
			.AuditId           = 12,
			.TerminalId        = 12,
		}
	));
	DemoUsers.insert(std::make_pair(
		"test_13",
		xDemoUser{
			.ControllerAddress = xNetAddress::Parse("127.0.0.1:10020"),
			.AuditId           = 13,
			.TerminalId        = 13,
		}
	));
	DemoUsers.insert(std::make_pair(
		"test_14",
		xDemoUser{
			.ControllerAddress = xNetAddress::Parse("127.0.0.1:10020"),
			.AuditId           = 14,
			.TerminalId        = 14,
		}
	));
});
