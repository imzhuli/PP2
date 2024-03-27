#include "../common/base.hpp"
#include "./audit.hpp"
#include "./auth.hpp"

auto IoCtx    = xIoContext();
auto Ticker   = xTicker();
auto RunState = xRunState();

int main(int argc, char ** argv) {

	auto RSG = xScopeGuard([] { RunState.Start(); }, [] { RunState.Finish(); });

	while (RunState) {
		Ticker.Update();
		OutputAudit(Ticker);
	}
	return 0;
}
