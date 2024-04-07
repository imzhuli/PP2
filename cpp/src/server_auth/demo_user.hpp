#pragma once
#include "../common/base.hpp"

struct xDemoUser {
	xNetAddress ControllerAddress;
	int32_t     AuditId;
	uint64_t    TerminalId;
};

extern std::unordered_map<std::string, xDemoUser> DemoUsers;
