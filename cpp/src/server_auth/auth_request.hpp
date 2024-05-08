#pragma once
#include "../common_job/async_request.hpp"

struct xAuthRequestSource {
	uint64_t RequestId;
};

struct xAuthRequest : xAuthRequestSource {
	std::string Account;
	std::string Password;
};

struct xAuthResult {
	uint64_t    AuditKey;                   // 绑定的计量账号(不是计费)
	xNetAddress TerminalControllerAddress;  // relay server, or terminal service address
	uint64_t    TerminalId;                 // index in relay server
	bool        EnableUdp;
};
