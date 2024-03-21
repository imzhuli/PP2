#include "../common/base.hpp"

class xHttpServer;
class xHttpServerDelegate;

struct xHttpServerOptions final {

	static constexpr const size_t LWS_SERVICE_CONNECTION_POOL_SIZE   = 100'000;
	static constexpr const size_t LWS_SERVICE_WRITE_BUFFER_POOL_SIZE = 100'000;

	xNetAddress BindAddress;
	std::string Hostname;
	std::string CertFilePath;
	std::string PriKeyFilePath;
	size_t      ConnectionPoolSize  = LWS_SERVICE_CONNECTION_POOL_SIZE;
	size_t      WriteBufferPoolSize = LWS_SERVICE_WRITE_BUFFER_POOL_SIZE;
};

enum struct eHttpMethod {
	GET     = 1,
	POST    = 2,
	OPTIONS = 3,
	PUT     = 4,
	DELETE  = 5,
	OTHER   = 1000,
};

using xHttpHeaderList = std::vector<std::string>;

class xHttpRequest : xNonCopyable {
public:
	const xHttpHeaderList & Headers;
	const std::string &     Uri;
	const std::string &     Body;

	xHttpRequest(struct xPerSessionData * Pss);
	bool IsMethod(eHttpMethod Method) const;

private:
	xVariable wsi = {};

	static const std::string EmptyHeaderValue;
};

class xHttpServer : xNonCopyable {
	friend class xHttpServerDelegate;

public:
	static constexpr size_t HttpBodyBufferSize = 600'000;

	struct xPssListNode : xListNode {};
	using xPssList = xList<xPssListNode>;

	struct xHttpSendBlockListNode : xListNode {};
	using xHttpSendBlockList = xList<xHttpSendBlockListNode>;

public:
	bool Init(const xHttpServerOptions & Options);
	void Clean();

	void Run();
	void Notify();
	void Stop();

	// thread safe:
	void PostData(uint64_t ConnectionId, const char * ContentType, const void * DataPtr, size_t Size, bool ShouldClose = true);  // in other threads
	void PostKillConnection(uint64_t ConnectionId);

protected:  // callbacks
	virtual void OnTick() {
		Pass();
	}

	virtual xArrayView<std::string> GetInterestedHeaders() {
		return {};
	}

	// return value:
	// true: http process finished.
	// false: there is any pending async operation
	virtual bool OnHttpRequest(uint64_t ConnectionId, const xHttpRequest & HttpRequest) {
		const char * r200 = "<html><head></head><body>200</body></html>";
		PostData(ConnectionId, "text/html", r200, strlen(r200));
		return true;  // request done
	}

	// virtual void OnCloseConnection(uint64_t ConnectionId) {
	// 	Pass();
	// }
	// virtual bool OnBinary(uint64_t ConnectionId, const ubyte * DataPtr, size_t DataSize);
	// virtual bool OnText(uint64_t ConnectionId, const ubyte * DataPtr, size_t DataSize);

	void KillConnection(uint64_t ConnectionId);

private:
	struct xHttpSendBlock {
		xHttpSendBlockListNode PendingWriteNode;
		uint64_t               IndexId      = 0;
		uint64_t               ConnectionId = 0;
		const char *           ContentType  = "binary/octet-stream";
		size_t                 DataSize     = 0;
		bool                   ShouldClose  = false;
		ubyte                  ReservedForPRE[24];
		ubyte                  BodyBuffer[HttpBodyBufferSize];
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
