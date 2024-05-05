#pragma once
#include "../common/base.hpp"

struct xRequestContext {
	uint64_t ConnectionId;
	uint64_t RequestId;
};

class xRequestContextService : xNonCopyable {
private:
	struct xRequestContextNode : xListNode {
		uint64_t        IndexId;
		uint64_t        TimestampMS;
		xRequestContext Context;
	};

public:
	xRequestContextService() {
		RuntimeAssert(RCManager.Init(5000));
	}
	~xRequestContextService() {
		RCManager.Clean();
	}

	uint64_t Put(uint64_t ConnectionId, uint64_t RequestId) {
		auto IndexId = RCManager.Acquire();
		if (!IndexId) {
			return 0;
		}
		auto & RC      = RCManager[IndexId];
		RC.IndexId     = IndexId;
		RC.TimestampMS = Ticker;

		RC.Context.ConnectionId = ConnectionId;
		RC.Context.RequestId    = RequestId;
		return IndexId;
	}

	xRequestContext GetAndRelease(uint64_t IndexId) {
		auto RCP = RCManager.CheckAndGet(IndexId);
		if (!RCP) {
			return {};
		}
		auto Ret = RCP->Context;

		assert(IndexId == RCP->IndexId);
		RCManager.Release(IndexId);
		return Ret;
	}

	void Tick() {
		Ticker.Update();
		auto C = [KillTimepoint = Ticker() - 1'500](const xRequestContextNode & N) { return N.TimestampMS < KillTimepoint; };
		while (auto NP = TimeoutList.PopHead(C)) {
			RCManager.Release(NP->IndexId);
		}
	}

private:
	xTicker                              Ticker;
	xIndexedStorage<xRequestContextNode> RCManager;
	xList<xRequestContextNode>           TimeoutList;
};
