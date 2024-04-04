#include "../common/base.hpp"
#include "../common/config.hpp"
#include "./audit.hpp"
#include "./auth.hpp"
#include "./proxy_access.hpp"

auto IoCtx    = xIoContext();
auto Ticker   = xTicker();
auto RunState = xRunState();

auto ClientService     = xProxyService();
auto BindAddress       = xNetAddress();
auto DispatcherAddress = xNetAddress();

int main(int argc, char ** argv) {

	auto CL = xCommandLine(
		argc, argv,
		{
			{ 'c', nullptr, "config", true },
		}
	);
	auto ConfigOpt = CL["config"];
	if (!ConfigOpt()) {
		cerr << "missing -c config_filename" << endl;
		return -1;
	}
	auto C = xConfig(ConfigOpt->c_str());
	C.Require(BindAddress, "bind_address");
	C.Require(DispatcherAddress, "dispatcher_address");

	auto RSG = xScopeGuard([] { RunState.Start(); }, [] { RunState.Finish(); });
	auto ICG = xResourceGuard(IoCtx);
	RuntimeAssert(ICG);
	auto CSG = xResourceGuard(ClientService, &IoCtx, BindAddress, DispatcherAddress);
	RuntimeAssert(CSG);

	while (RunState) {
		Ticker.Update();
		IoCtx.LoopOnce();
		ClientService.Tick(Ticker);

		// audit:
		ProxyAudit.ClientConnectionCount = ClientService.GetAuditClientConnectionCount();
		OutputAudit(Ticker);
	}
	return 0;
}
