#include "./dns_server.hpp"

#include "../common_protocol/network.hpp"

static std::vector<xPacketCommandId> InterestedCommandIds = { Cmd_HostQuery };

static bool IsLocal(const xNetAddress & Addr) {
	if (Addr.IsV4()) {
		ubyte B0 = Addr.SA4[0];
		ubyte B1 = Addr.SA4[1];
		if (B0 == 10) {
			return true;
		}
		if (B0 == 192 && B1 == 168) {
			return true;
		}
		if (B0 == 172 && (B1 >= 16 && B1 < 31)) {
			return true;
		}
	} else if (Addr.IsV6()) {
		ubyte B0 = Addr.SA6[0];
		ubyte B1 = Addr.SA6[1];
		if ((B0 & 0xFE) == 0xFC) {
			return true;
		}
		if (B0 == 0xFE && (B1 & 0xC0 == 0x80)) {
			return true;
		}
	}
	return false;
}

void xDnsServer::OnLocalDnsWakeup(xVariable Ctx) {
	auto DSP = static_cast<xDnsServer *>(Ctx.P);
	assert(DSP);
	DSP->IoContext.Interrupt();
}

bool xDnsServer::Init(xNetAddress DispatcherAddress, size_t MaxRequestCount) {
	RuntimeAssert(IoContext.Init());
	RuntimeAssert(xClient::Init(&IoContext, DispatcherAddress));
	RuntimeAssert(xDnsRequestManager::Init(MaxRequestCount));
	RuntimeAssert(LocalDnsService.Init(OnLocalDnsWakeup, { .P = this }));
	return true;
}

void xDnsServer::Clean() {
	LocalDnsService.Clean();
	xDnsRequestManager::Clean();
	xClient::Clean();
	IoContext.Clean();
}

void xDnsServer::OnLocalDnsResult(xVariable CallbackCtx, xVariable RequestCtx, const xNetAddress & A4, const xNetAddress & A6) {
	auto SP               = static_cast<xDnsServer *>(CallbackCtx.P);
	auto RequestContextId = RequestCtx.U64;

	auto CacheNode = xDnsResult();
	CacheNode.A4   = A4;
	CacheNode.A6   = A6;
	SP->SetData(RequestContextId, CacheNode);
}

void xDnsServer::Tick() {
	Ticker.Update();
	IoContext.LoopOnce();
	xClient::Tick(Ticker);
	LocalDnsService.GetAndDispatchResult(OnLocalDnsResult, { .P = this });
	xDnsRequestManager::Tick(Ticker);
}

void xDnsServer::OnServerConnected() {
	X_DEBUG_PRINTF("");
	ubyte Buffer[MaxPacketSize];
	auto  RSize = xPacket::MakeRegisterDispatcherConsumer(Buffer, sizeof(Buffer), InterestedCommandIds.data(), InterestedCommandIds.size());
	PostData(Buffer, RSize);
}

bool xDnsServer::OnPacket(const xPacketHeader & Header, ubyte * PayloadPtr, size_t PayloadSize) {
	X_DEBUG_PRINTF("Request: RID=%" PRIx64 "", Header.RequestId);
	if (Cmd_HostQuery != Header.CommandId) {
		X_DEBUG_PRINTF("Invalid request command");
		return true;
	}
	auto R = xHostQueryReq();
	if (!R.Deserialize(PayloadPtr, PayloadSize) || R.Hostname.empty()) {
		X_DEBUG_PRINTF("Invalid request format");
		return false;
	}
	auto DR      = xDnsRequest{};
	DR.RequestId = Header.RequestId;
	DR.Hostname  = R.Hostname;
	Query(DR.Hostname, DR);
	return true;
}

void xDnsServer::PostRequest(uint64_t RequestId, const xRequestSource & Source) {
	auto & Request = static_cast<const xDnsRequest &>(Source);
	LocalDnsService.Query(Request.Hostname, { .U64 = RequestId });
}

void xDnsServer::PostResult(const xRequestSource & Source, bool HasData, const xDataNode & Data) {
	auto R = xHostQueryResp{};
	if (!IsLocal(Data.A4)) {
		R.Addr4 = Data.A4;
	}
	if (!IsLocal(Data.A6)) {
		R.Addr6 = Data.A6;
	}
	ubyte Buffer[MaxPacketSize];
	auto  RSize = WritePacket(Cmd_HostQueryResp, Source.RequestId, Buffer, sizeof(Buffer), R);
	PostData(Buffer, RSize);
}

// void xDnsService::Tick() {
// 	IoContext.LoopOnce();
// 	xDnsRequestManager::Tick();
// 	// get and dispatch results:
// 	DispatchResults();
// }

// void xDnsService::DispatchResults() {
// 	xJobList FJL = {};
// 	do {
// 		auto G = xSpinlockGuard(FinishedJobListLock);
// 		FJL.GrabListTail(FinishedJobList);
// 	} while (false);
// 	while (auto PJ = FJL.PopHead()) {
// 		auto & Job      = static_cast<xDnsJob &>(*PJ);
// 		auto   ReqIndex = Job.JobCtx.U64;
// 		auto   ReqPtr   = RequestPool.CheckAndGet(ReqIndex);
// 		X_DEBUG_PRINTF("ReqPtr=%p, Id=%" PRIx64 "", ReqPtr, ReqIndex);
// 		if (ReqPtr) {
// 			const xNetAddress * PA4 = &Job.A4;
// 			const xNetAddress * PA6 = &Job.A6;
// 			if (!EnableLocal) {
// 				if (IsLocal(*PA4)) {
// 					PA4 = &InvalidNetAddress;
// 				}
// 				if (IsLocal(*PA6)) {
// 					PA6 = &InvalidNetAddress;
// 				}
// 			}
// 			Callback(ReqPtr->Context, *PA4, *PA6);
// 			RequestPool.Release(ReqIndex);
// 		}
// 		DeleteDnsJob(&Job);
// 	}
// 	// try remove timeout requests:
// 	DnsReqTicker.Update();
// 	auto C = [KillTimepoint = DnsReqTicker - DnsReqTimeoutMS](const xDnsReq & N) { return N.Timestamp < KillTimepoint; };
// 	while (auto NP = DnsReqList.PopHead(C)) {
// 		assert(NP == RequestPool.CheckAndGet(NP->Index));
// 		Callback(NP->Context, {}, {});
// 		RequestPool.Release(NP->Index);
// 	}
// }
