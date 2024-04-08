#include "./base.hpp"

#include <crypto/sha.hpp>

std::string DebugSign(const void * DataPtr, size_t Size) {
	auto D = Sha256(DataPtr, Size);
	return StrToHex(D.Digest, 32);
}
