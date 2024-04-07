#include "./terminal_relay.hpp"

static auto IoCtx    = xIoContext();
static auto RunState = xRunState();
static auto Relay    = xTerminalRelay();

int main(int argc, char ** argv) {
	auto CL = xCommandLine(
		argc, argv,
		{
			{ 'a', nullptr, "bind_address", true },
			{ 'i', nullptr, "ip_table_file", true },
		}
	);

	auto BindAddressOpt = CL["bind_address"];
	if (!BindAddressOpt()) {
		X_PERROR("missing option: -a bind_address");
		return -1;
	}
	auto BindAddress = xNetAddress::Parse(*BindAddressOpt);

	auto IpTableFileOpt = CL["ip_table_file"];
	if (!IpTableFileOpt()) {
		X_PERROR("missing option: -i ip_table_file");
		return -1;
	}

	auto ICG = xResourceGuard(IoCtx);
	RuntimeAssert(ICG);
	auto DRG = xResourceGuard(Relay, &IoCtx, BindAddress, IpTableFileOpt->c_str());
	RuntimeAssert(DRG);
	auto RunStateGuard = xScopeGuard([&] { RunState.Start(); }, [] { RunState.Finish(); });

	auto Ticker = xTicker();
	while (RunState) {
		Ticker.Update();
		IoCtx.LoopOnce();
		Relay.Tick(Ticker);
	}

	return 0;
}
