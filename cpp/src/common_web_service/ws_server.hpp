#include "../common/base.hpp"

class xWsServer;
class xWsServerDelegate;

struct xWsServerOptions final {
	static constexpr const size_t LWS_SERVICE_CONNECTION_POOL_SIZE   = 100'000;
	static constexpr const size_t LWS_SERVICE_WRITE_BUFFER_POOL_SIZE = 100'000;

	xNetAddress BindAddress;
	std::string Hostname;
	std::string CertFilePath;
	std::string PriKeyFilePath;
	size_t      ConnectionPoolSize  = LWS_SERVICE_CONNECTION_POOL_SIZE;
	size_t      WriteBufferPoolSize = LWS_SERVICE_WRITE_BUFFER_POOL_SIZE;
};

class xWsServer : xNonCopyable {
	friend class xWsServerDelegate;

public:
	static constexpr size_t WsSendBlockBufferSize = 2000;

	struct xPssListNode : xListNode {};
	using xPssList = xList<xPssListNode>;

	struct xHttpSendBlockListNode : xListNode {};
	using xHttpSendBlockList = xList<xHttpSendBlockListNode>;

public:
	bool Init(const xWsServerOptions & Options);
	void Clean();

	void Run();
	void Notify();
	void Stop();

	// thread safe:
	void PostData(uint64_t ConnectionId, const void * DataPtr, size_t Size, bool Binary = true);  // in other threads
	void PostKillConnection(uint64_t ConnectionId);

protected:  // callbacks
	virtual void OnTick() {
		Pass();
	}
	virtual bool OnNewConnection(uint64_t ConnectionId, const xNetAddress & Address) {
		return true;
	}
	virtual void OnCloseConnection(uint64_t ConnectionId) {
		Pass();
	}
	virtual bool OnBinary(uint64_t ConnectionId, const ubyte * DataPtr, size_t DataSize);
	virtual bool OnText(uint64_t ConnectionId, const char * DataPtr, size_t DataSize);

	void KillConnection(uint64_t ConnectionId);

private:
	struct xHttpSendBlock {
		xHttpSendBlockListNode PendingWriteNode;
		uint64_t               IndexId      = 0;
		uint64_t               ConnectionId = 0;
		size_t                 DataSize     = 0;
		union {
			bool  Binary;
			ubyte ReservedForPRE[24];
		};
		ubyte Buffer[WsSendBlockBufferSize];
	};
	using xHttpSendBlockDummy = xDummy<sizeof(xHttpSendBlock)>;
	static_assert(std::is_standard_layout_v<xHttpSendBlock>);
	static_assert(std::is_standard_layout_v<xHttpSendBlockDummy>);
	static_assert(std::is_trivially_constructible_v<xHttpSendBlockDummy>);
	static_assert(std::is_trivially_destructible_v<xHttpSendBlockDummy>);

	xHttpSendBlock * New(size_t DataSize);
	void             Delete(xHttpSendBlock *);

	bool InitPss(struct xPerSessionData * Pss);
	void CleanPss(struct xPerSessionData * Pss);

private:
	xNetAddress                          BindAddress = {};
	xIndexedStorage<xVariable>           ConnectionIdPool;
	xIndexedStorage<xHttpSendBlockDummy> SmallBlockBufferPool;
	xVariable                            LwsContext;

	xSpinlock                    AllocationLock;
	xPssList                     PssList;
	xHttpSendBlockList           PendingWriteList;
	xSpinlock                    PendingWriteSyncLock;
	xRunState                    RunState;
	std::atomic<std::thread::id> RunThreadId;
};
