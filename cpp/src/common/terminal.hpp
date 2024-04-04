#pragma once
#include "./base.hpp"
#include "./region.hpp"

struct xTerminalInfo {
	uint64_t xTerminalId = 0;  // internal usage

	xRegionId   RegionId;
	xNetAddress TerminalAddress;             // terminal out address
	xNetAddress TerminalControllerAddress;   // relay server, or terminal service address
	uint64_t    TerminalControllerSubIndex;  // index in relay server
};
