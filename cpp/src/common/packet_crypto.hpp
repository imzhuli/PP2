#pragma once
#include "./base.hpp"

struct xPacketCrypto {
	ubyte  Target[MaxPacketPayloadSize];
	size_t TargetSize = 0;
};
