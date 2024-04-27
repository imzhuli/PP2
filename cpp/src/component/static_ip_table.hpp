#pragma once
#include "../common/base.hpp"

#include <unordered_map>

struct xStaticIpRecord {
	xNetAddress TargetIp;
	xNetAddress TerminalControllerAddress;
	uint64_t    TerminalId;
};

using xStaticIpTable = std::unordered_map<std::string /* ip string */, xStaticIpRecord>;

extern xStaticIpTable LoadStaticIpTable(const char * Filename);
