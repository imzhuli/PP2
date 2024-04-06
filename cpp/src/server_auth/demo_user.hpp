#pragma once
#include "../common/base.hpp"

struct xDemoUser {
	int32_t     AuditId;
	xNetAddress ControllerAddress;
	uint64_t    TerminalId;
};

extern std::unordered_map<std::string, xDemoUser> DemoUsers;
