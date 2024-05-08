#pragma once
#include "../common/base.hpp"

#include <unordered_map>

template <typename tRequestSource, typename tDataNode, typename tUpdateKey>
struct xAsyncRequest : xListNode {

	enum eState {
		NEW      = 0,
		QUERYING = 1,
		NO_DATA  = 2,
		READY    = 3,
	};
	struct xDispatcher : xListNode {
		eState                      State = NEW;
		tDataNode                   DataNode;
		std::vector<tRequestSource> RequestSources;
	};

	xIndexId    RequestId = {};
	std::string CacheKey;
	uint64_t    CacheTimestampMS;
	xDispatcher Dispatcher;

	static_assert(!std::is_reference_v<tUpdateKey>);
	static_assert(!std::is_reference_v<tRequestSource>);
	static_assert(!std::is_reference_v<tDataNode>);

	static_assert(std::is_default_constructible_v<tUpdateKey> && std::is_copy_assignable_v<tUpdateKey>);
	static_assert(std::is_copy_constructible_v<tRequestSource> || std::is_move_constructible_v<tRequestSource>);
	static_assert(std::is_default_constructible_v<tDataNode> && std::is_copy_assignable_v<tDataNode>);
};

template <typename tRequestSource, typename tDataNode, typename tUpdateKey>
using xAsyncRequestList = xList<xAsyncRequest<tRequestSource, tDataNode, tUpdateKey>>;

template <typename tRequestSource, typename tDataNode, typename tUpdateKey = std::string>
class xAsyncRequestManager {
protected:
	using xUpdateKey     = std::remove_cv_t<tUpdateKey>;
	using xRequestSource = std::remove_cv_t<tRequestSource>;
	using xDataNode      = std::remove_cv_t<tDataNode>;
	using xRequest       = xAsyncRequest<xRequestSource, xDataNode, xUpdateKey>;
	using xRequestList   = xList<xRequest>;
	static_assert(std::is_same_v<xRequestList, xAsyncRequestList<xRequestSource, xDataNode, xUpdateKey>>);

public:
	bool Init(size_t MaxRequestNodes) {
		if (!RequestNodeManager.Init(MaxRequestNodes)) {
			return false;
		}
		return true;
	}

	void Clean() {
		RequestNodeManager.Clean();
	}

	bool Query(const xUpdateKey & UpdateKey, const xRequestSource & Source) {
		X_DEBUG_PRINTF("Using Key: %s", StrToHex(UpdateKey).c_str());
		auto RP = GetOrCreateRequest(UpdateKey, Source);
		if (!RP) {
			return false;
		}
		RP->Dispatcher.RequestSources.push_back(Source);
		switch (RP->Dispatcher.State) {
			case xRequest::NEW:
				PostRequest(RP->RequestId, Source);
				RP->Dispatcher.State = xRequest::QUERYING;
				return true;
			case xRequest::QUERYING:
				return true;
			case xRequest::NO_DATA:
			case xRequest::READY:
				X_DEBUG_PRINTF("Using Cached Data");
				DispatcherList.GrabTail(RP->Dispatcher);
				return true;
			default:
				X_DEBUG_PRINTF("Invalid async request state");
				break;
		}
		// error:
		DestroyRequest(RP);
		return false;
	}

	X_INLINE void Tick() {
		Tick(GetTimestampUS());
	}

	X_INLINE void Tick(uint64_t NowMS) {
		Ticker.Update(NowMS);
		DispatchResults();
		// clear timeout nodes:
		auto Cond = [KillTimepoint = Ticker() - CacheTimeoutMS](const xRequest & R) { return R.CacheTimestampMS <= KillTimepoint; };
		while (auto RP = CacheTimeoutList.PopHead(Cond)) {
			X_DEBUG_PRINTF("Remove node: %s", RP->CacheKey.c_str());
			DestroyRequest(RP);
		}
	}

protected:
	virtual void PostRequest(uint64_t RequestId, const xRequestSource & Source)                  = 0;
	virtual void PostResult(const xRequestSource & Source, bool HasData, const xDataNode & Data) = 0;

	X_INLINE void SetNoData(xIndexId RequestId) {
		auto RP = RequestNodeManager.CheckAndGet(RequestId);
		if (!RP) {
			return;
		}
		RP->Dispatcher.State = xRequest::NO_DATA;
		Reset(RP->Dispatcher.DataNode);
		DispatcherList.GrabTail(RP->Dispatcher);
	}

	X_INLINE void SetData(xIndexId RequestId, const xDataNode & Data) {
		auto RP = RequestNodeManager.CheckAndGet(RequestId);
		if (!RP) {
			return;
		}
		RP->Dispatcher.State    = xRequest::READY;
		RP->Dispatcher.DataNode = Data;
		DispatcherList.GrabTail(RP->Dispatcher);
	}

	X_INLINE void SetData(xIndexId RequestId, xDataNode && Data) {
		auto RP = RequestNodeManager.CheckAndGet(RequestId);
		if (!RP) {
			return;
		}
		RP->Dispatcher.State    = xRequest::READY;
		RP->Dispatcher.DataNode = std::move(Data);
		DispatcherList.GrabTail(RP->Dispatcher);
	}

private:
	X_INLINE xRequest * GetOrCreateRequest(const xUpdateKey & Key, const xRequestSource & Source) {
		auto & RP = CacheNodeMap[Key];
		if (!RP) {
			auto RID = RequestNodeManager.Acquire();
			if (X_UNLIKELY(!RID)) {
				CacheNodeMap.erase(Key);
				return nullptr;
			}
			RP                   = &RequestNodeManager[RID];
			RP->RequestId        = RID;
			RP->CacheKey         = Key;
			RP->CacheTimestampMS = Ticker();
			CacheTimeoutList.AddTail(*RP);
			return RP;
		}
		return RP;
	}

	X_INLINE void DestroyRequest(xRequest * RequestPtr) {
		assert(RequestPtr == RequestNodeManager.CheckAndGet(RequestPtr->RequestId));
		CacheNodeMap.erase(RequestPtr->CacheKey);
		RequestNodeManager.Release(RequestPtr->RequestId);
	}

	X_INLINE void DispatchResults() {
		while (auto DP = DispatcherList.PopHead()) {
			auto HasData = (DP->State == xRequest::READY);
			for (const auto & R : DP->RequestSources) {
				PostResult(R, HasData, DP->DataNode);
			}
			DP->RequestSources.clear();
		}
	}

private:
	xTicker                                    Ticker;
	uint64_t                                   CacheTimeoutMS = 5 * 60'000;
	xRequestList                               CacheTimeoutList;
	xList<typename xRequest::xDispatcher>      DispatcherList;
	xIndexedStorage<xRequest>                  RequestNodeManager;
	std::unordered_map<xUpdateKey, xRequest *> CacheNodeMap;
};
