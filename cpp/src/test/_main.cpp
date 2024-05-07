#include "../common/base.hpp"
#include "../common_job/async_request.hpp"

struct xAuthRequestSource {
	uint64_t RequestId;

	xAuthRequestSource(xAuthRequestSource &&)      = default;
	xAuthRequestSource(const xAuthRequestSource &) = delete;
};

struct xAuthResult {
	uint64_t    AuditKey;                   // 绑定的计量账号(不是计费)
	xNetAddress TerminalControllerAddress;  // relay server, or terminal service address
	uint64_t    TerminalId;                 // index in relay server
	bool        EnableUdp;
};

class xAuthRequestManager : public xAsyncRequestManager<xAuthRequestSource, xAuthResult> {
	void UpdateRequest(const xIndexId RequestId, const xUpdateKey & UpdateKey) override {
	}
	void DispatchResult(const xRequestSource & Source, const xDataNode & Data) override {
	}
};

int main(int argc, char ** argv) {
	auto X = xAuthRequestManager();

	Touch(X);
	return 0;
}
