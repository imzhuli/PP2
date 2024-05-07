#pragma once
#include "../common/base.hpp"

#include <unordered_map>

template <typename tRequestSource, typename tDataNode, typename tUpdateKey>
struct xAsyncRequest : xListNode {
	xIndexId                    RequestId;
	uint64_t                    TimestampMS;
	tUpdateKey                  UpdateKey;
	tDataNode                   DataNode;
	std::vector<tRequestSource> RequestSources;

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
	using xRequestList   = xAsyncRequestList<xRequestSource, xDataNode, xUpdateKey>;

public:
	bool Init(size_t MaxRequestNodes) {
		Todo("");
		return false;
	}
	void Clean() {
		Todo("");
		return;
	}
	bool PostRequest(const xUpdateKey & UpdateKey, xRequestSource & Source) {
		Todo("");
		return false;
	}
	virtual void UpdateRequest(const xIndexId RequestId, const xUpdateKey & UpdateKey) = 0;
	virtual void DispatchResult(const xRequestSource & Source, const xDataNode & Data) = 0;

protected:
	xIndexId GetOrCreateRequest(xRequestSource & Source) {
		Todo("");
	}
	void DestroyRequest(xRequest * RequestPtr) {
		Todo("");
	}

	X_INLINE void PutResult(xIndexId RequestId, const xDataNode & Data) {
		auto RP = CacheNodeManager.CheckAndGet(RequestId);
		if (!RP) {
			return;
		}
		RP->DataNode = Data;
	}

private:
	xTicker                                    Ticker;
	uint64_t                                   CacheTimeoutMS = 5 * 60'000;
	xRequestList                               CacheTimeoutList;
	uint64_t                                   RequestTimeoutMS = 2'000;
	xRequestList                               RequestTimeoutList;
	xIndexedStorage<xRequest>                  CacheNodeManager;
	std::unordered_map<xUpdateKey, xRequest *> CacheNodeMap;
};
