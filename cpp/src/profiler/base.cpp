#include "./base.hpp"

#include <sstream>

std::string xConnectionProfile::ToString() const {
	char Buffer[256];
	// clang-format off
    int S = snprintf(Buffer, sizeof(Buffer),
        "Start:%" PRIu64 ","
        "Delay:%" PRIi64 ","
        "Duration:%" PRIi64 ","
        "Up:%" PRIu64 ","
        "Down:%" PRIu64 "",
        Start,
        std::max(MakeSigned(ConnectionDelay), ssize64_t(-1)),
        std::max(MakeSigned(ConnectionDuration), ssize64_t(-1)),
        UploadSize,
        DownloadSize
        );
	// clang-format on
	return std::string(Buffer, S);
}
