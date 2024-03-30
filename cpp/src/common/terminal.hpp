#pragma once
#include "./base.hpp"
#include "./region.hpp"

struct xTerminalInfo {
	uint64_t xTerminalId = 0;

	xRegionId   RegionId;
	xNetAddress TerminalAddress;            // terminal out address;
	xNetAddress TerminalControllerAddress;  // relay server, or terminal service address
};
