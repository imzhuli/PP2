#pragma once
#include "../common/base.hpp"

struct xProfile {
	size_t Total      = 0;
	size_t DurationMS = 0;
};

class xProfiler : xNonCopyable {
public:
	xProfiler() = default;
	xProfiler(const std::string & Name)
		: Name(Name) {
	}

	const std::string & GetName() const {
		return Name;
	}

	void Hit(size_t Step = 1) {
		Counter += Step;
	}

	xProfile Dump() {
		uint64_t Last     = Ticker();
		uint64_t Duration = Ticker.Update() - Last;
		return xProfile{
			.Total      = Steal(Counter),
			.DurationMS = Duration,
		};
	}

private:
	std::string Name;
	xTicker     Ticker  = {};
	size_t      Counter = 0;
};
