#include "./demo_user.hpp"

// 45.205.98.10

std::unordered_map<std::string, xDemoUser> DemoUsers;

auto Init = xInstantRun([] {
	DemoUsers.insert(std::make_pair(
		"test_10",
		xDemoUser{
			.AuditId           = 10,
			.ControllerAddress = xNetAddress::Parse("127.0.0.1:10020"),
			.ControllerIndexId = 10,
		}
	));
	DemoUsers.insert(std::make_pair(
		"test_11",
		xDemoUser{
			.AuditId           = 11,
			.ControllerAddress = xNetAddress::Parse("127.0.0.1:10020"),
			.ControllerIndexId = 11,
		}
	));

	DemoUsers.insert(std::make_pair(
		"test_12",
		xDemoUser{
			.AuditId           = 12,
			.ControllerAddress = xNetAddress::Parse("127.0.0.1:10020"),
			.ControllerIndexId = 12,
		}
	));
	DemoUsers.insert(std::make_pair(
		"test_13",
		xDemoUser{
			.AuditId           = 13,
			.ControllerAddress = xNetAddress::Parse("127.0.0.1:10020"),
			.ControllerIndexId = 13,
		}
	));
	DemoUsers.insert(std::make_pair(
		"test_14",
		xDemoUser{
			.AuditId           = 14,
			.ControllerAddress = xNetAddress::Parse("127.0.0.1:10020"),
			.ControllerIndexId = 14,
		}
	));
});
