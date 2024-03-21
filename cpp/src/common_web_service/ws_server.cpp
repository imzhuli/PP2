#include "./ws_server.hpp"

#include <libwebsockets.h>

#include <core/core_value_util.hpp>

// defaults:
static constexpr int LWS_SHOULD_CLOSE = 1;

// libwebsockets callbacs:
struct xPerSessionData : xWsServer::xPssListNode {
	struct lws *                  wsi          = nullptr;
	bool                          Writing      = false;
	bool                          Killing      = false;
	uint64_t                      ConnectionId = 0;
	xNetAddress                   ClientAddress;
	xWsServer::xHttpSendBlockList SendBlockList;
};

class xWsServerDelegate {
public:
	static_assert(sizeof(xWsServer::xHttpSendBlock::ReservedForPRE) >= LWS_PRE);
	static int LwsCallback(struct lws * wsi, enum lws_callback_reasons reason, void * session, void * in, size_t len);
};

int xWsServerDelegate::LwsCallback(struct lws * wsi, enum lws_callback_reasons reason, void * session, void * in, size_t len) {
	auto ContextPtr = lws_get_context(wsi);
	auto ServerPtr  = (xWsServer *)lws_context_user(ContextPtr);
	auto Pss        = (xPerSessionData *)session;
	switch (reason) {

		case LWS_CALLBACK_ESTABLISHED: {
			if (!ServerPtr->InitPss(Pss)) {
				return LWS_SHOULD_CLOSE;
			}
			Pss->wsi = wsi;

			//  auto Addr = sockaddr_storage();
			char AddressBuffer[256] = {};
			lws_get_peer_simple(wsi, AddressBuffer, sizeof(AddressBuffer));
			Pss->ClientAddress = xNetAddress::Parse(AddressBuffer);

			if (!ServerPtr->OnNewConnection(Pss->ConnectionId, Pss->ClientAddress)) {
				return LWS_SHOULD_CLOSE;
			}
			X_DEBUG_PRINTF("LWS_CALLBACK_ESTABLISHED WSI: %p, %p, ConnectionId=%" PRIx64 "", wsi, Pss, Pss->ConnectionId);
			break;
		}

		case LWS_CALLBACK_CLOSED:
			X_DEBUG_PRINTF("LWS_CALLBACK_CLOSED WSI: %p, %p, ConnectionId=%" PRIx64 "", wsi, Pss, Pss->ConnectionId);
			if (!Pss || !Pss->wsi) {  // initialization not completed
				break;
			}
			ServerPtr->OnCloseConnection(Pss->ConnectionId);
			ServerPtr->CleanPss(Pss);
			break;

		// data events
		case LWS_CALLBACK_RECEIVE:
			// X_DEBUG_PRINTF("LWS_CALLBACK_RECEIVE, Pss=%p, ConnectionId=%" PRIx64 ", message=\n%s", Pss, Pss->ConnectionId, HexShow(in, len).c_str());
			if (lws_frame_is_binary(wsi)) {
				if (!ServerPtr->OnBinary(Pss->ConnectionId, (const ubyte *)in, len)) {
					return LWS_SHOULD_CLOSE;
				}
			} else {
				if (!ServerPtr->OnText(Pss->ConnectionId, (const char *)in, len)) {
					return LWS_SHOULD_CLOSE;
				}
			}
			break;

		case LWS_CALLBACK_SERVER_WRITEABLE: {
			// X_DEBUG_PRINTF("LWS_CALLBACK_SERVER_WRITEABLE: %p, %p, ConnectionId=%" PRIx64 "", wsi, Pss, Pss->ConnectionId);
			auto NodePtr = Pss->SendBlockList.Head();
			if (!NodePtr) {
				assert(!Pss->Writing);
				break;
			}

			auto BlockPtr = X_Entry(NodePtr, xWsServer::xHttpSendBlock, PendingWriteNode);
			if (Pss->Writing) {
				ServerPtr->Delete(BlockPtr);
				NodePtr = Pss->SendBlockList.Head();
			}
			if (!(Pss->Writing = NodePtr)) {
				break;
			}
			BlockPtr = X_Entry(NodePtr, xWsServer::xHttpSendBlock, PendingWriteNode);
			// X_DEBUG_PRINTF("Trying to send: BlockPtr=%p\n%s", BlockPtr, HexShow(BlockPtr->Buffer, BlockPtr->DataSize).c_str());
			if (lws_write(wsi, BlockPtr->Buffer, BlockPtr->DataSize, (BlockPtr->Binary ? LWS_WRITE_BINARY : LWS_WRITE_TEXT)) < (int)BlockPtr->DataSize) {
				return LWS_SHOULD_CLOSE;
			}
			lws_callback_on_writable(wsi);
			break;
		}

		case LWS_CALLBACK_ADD_HEADERS: {
			// X_DEBUG_PRINTF("%s\n", "LWS_CALLBACK_ADD_HEADERS");
			struct lws_process_html_args * args = (struct lws_process_html_args *)in;
			if (lws_add_http_header_by_name(
					wsi, (unsigned char *)"Access-Control-Allow-Origin", (unsigned char *)"*", 1, (unsigned char **)&args->p,
					(unsigned char *)args->p + args->max_len
				)) {
				return LWS_SHOULD_CLOSE;
			}
			break;
		}
		//
		// clang-format off

		/* connection events */
		// CASE_PRINT(LWS_CALLBACK_WSI_CREATE);
		// CASE_PRINT(LWS_CALLBACK_SERVER_NEW_CLIENT_INSTANTIATED);
		// CASE_PRINT(LWS_CALLBACK_HTTP_CONFIRM_UPGRADE);
		// CASE_PRINT(LWS_CALLBACK_HTTP_BIND_PROTOCOL);
		// CASE_PRINT(LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION);
		// CASE_PRINT(LWS_CALLBACK_FILTER_NETWORK_CONNECTION);
		// CASE_PRINT(LWS_CALLBACK_WSI_DESTROY);

		/* local calls */
		// CASE_PRINT(LWS_CALLBACK_GET_THREAD_ID);
		// CASE_PRINT(LWS_CALLBACK_PROTOCOL_INIT);
		// CASE_PRINT(LWS_CALLBACK_EVENT_WAIT_CANCELLED);
		// CASE_PRINT(LWS_CALLBACK_WS_PEER_INITIATED_CLOSE);
		// CASE_PRINT(LWS_CALLBACK_WS_SERVER_DROP_PROTOCOL);
		// CASE_PRINT(LWS_CALLBACK_CLOSED_HTTP);

		// clang-format on
		//
		default:
			// X_DEBUG_PRINTF("other reason: %i", reason);
			break;
	}
	return 0;
}

[[maybe_unused]] static struct lws_protocols protocols[] = {
	{ "ws", xWsServerDelegate::LwsCallback, sizeof(xPerSessionData), MaxPacketSize, 0, nullptr, 0 },
	LWS_PROTOCOL_LIST_TERM,
};

////////////// Server //////////////

bool xWsServer::Init(const xWsServerOptions & Options) {

	if (!ConnectionIdPool.Init(Options.ConnectionPoolSize)) {
		return false;
	}
	auto ConnectionIdPoolCleaner = MakeResourceCleaner(ConnectionIdPool);

	if (!SmallBlockBufferPool.Init(Options.WriteBufferPoolSize)) {
		return false;
	}
	auto BufferPoolCleaner = MakeResourceCleaner(SmallBlockBufferPool);

	do {
		auto LwsContextInfo = lws_context_creation_info{};

		auto context = static_cast<lws_context *>(nullptr);

		memset(&LwsContextInfo, 0, sizeof LwsContextInfo); /* otherwise uninitialized garbage */
		LwsContextInfo.vhost_name = Options.Hostname.c_str();
		LwsContextInfo.port       = Options.BindAddress.Port;
		LwsContextInfo.protocols  = protocols;
		LwsContextInfo.options    = LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;
		LwsContextInfo.user       = this;

		if (Options.Hostname.size()) {
			LwsContextInfo.options |= LWS_SERVER_OPTION_VHOST_UPG_STRICT_HOST_CHECK;
		}

		if (Options.CertFilePath.size() && Options.PriKeyFilePath.size()) {
			lwsl_user("Server using TLS\n");
			LwsContextInfo.options                 |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
			LwsContextInfo.ssl_cert_filepath        = Options.CertFilePath.c_str();
			LwsContextInfo.ssl_private_key_filepath = Options.PriKeyFilePath.c_str();
		}

		context = lws_create_context(&LwsContextInfo);
		if (!context) {
			lwsl_err("lws init failed\n");
			return false;
		}
		LwsContext.P = context;
	} while (false);
	auto ContextCleaner = xScopeGuard([&] { lws_context_destroy((struct lws_context *)Steal(LwsContext.P)); });

	// successfully initialized
	ConnectionIdPoolCleaner.Dismiss();
	BufferPoolCleaner.Dismiss();
	ContextCleaner.Dismiss();
	return true;
}

void xWsServer::Clean() {
	assert(PssList.IsEmpty());
	lws_context_destroy((struct lws_context *)Steal(LwsContext.P));
	SmallBlockBufferPool.Clean();
	ConnectionIdPool.Clean();
}

void xWsServer::Run() {
	RunThreadId = std::this_thread::get_id();
	RuntimeAssert(RunState.Start());
	auto FinishGuard = xScopeGuard([this] { RunState.Finish(); });

	auto ContextPtr         = (struct lws_context *)LwsContext.P;
	auto RunThreadWriteList = xHttpSendBlockList();
	while (RunState) {
		RuntimeAssert(0 <= lws_service(ContextPtr, 0));
		//
		OnTick();
		do {  // some synchronizations
			auto G = xSpinlockGuard(PendingWriteSyncLock);
			RunThreadWriteList.GrabListTail(PendingWriteList);
		} while (false);
		// dispatch writes:
		for (auto & N : RunThreadWriteList) {
			auto BlockPtr = X_Entry(&N, xHttpSendBlock, PendingWriteNode);
			if (!BlockPtr->DataSize) {
				KillConnection(BlockPtr->ConnectionId);
				Delete(BlockPtr);
				continue;
			}
			auto PPss = ConnectionIdPool.CheckAndGet(BlockPtr->ConnectionId);
			if (!PPss) {
				Delete(BlockPtr);
				continue;
			}
			auto Pss = static_cast<xPerSessionData *>(PPss->P);
			Pss->SendBlockList.GrabTail(BlockPtr->PendingWriteNode);
			if (!Pss->Writing) {
				lws_callback_on_writable(Pss->wsi);
			}
		}
	}
	for (auto & N : RunThreadWriteList) {
		auto BlockPtr = X_Entry(&N, xHttpSendBlock, PendingWriteNode);
		Delete(BlockPtr);
	}
	for (auto & N : PssList) {
		CleanPss(static_cast<xPerSessionData *>(&N));
	}

	RunThreadId = std::thread::id();
}

void xWsServer::Notify() {
	auto context = (struct lws_context *)LwsContext.P;
	assert(context);
	lws_cancel_service(context);
}

void xWsServer::Stop() {
	RunState.Stop();
	Notify();
}

bool xWsServer::OnBinary(uint64_t ConnectionId, const ubyte * DataPtr, size_t DataSize) {
	PostData(ConnectionId, DataPtr, DataSize, true);
	return true;
}

bool xWsServer::OnText(uint64_t ConnectionId, const char * DataPtr, size_t DataSize) {
	PostData(ConnectionId, DataPtr, DataSize, false);
	return true;
}

void xWsServer::PostData(uint64_t ConnectionId, const void * DataPtr, size_t DataSize, bool Binary) {
	auto BlockPtr = New(DataSize);
	if (!BlockPtr) {
		return;
	}
	BlockPtr->ConnectionId = ConnectionId;
	BlockPtr->DataSize     = DataSize;
	BlockPtr->Binary       = Binary;
	memcpy(BlockPtr->Buffer, DataPtr, DataSize);

	auto G = xSpinlockGuard(PendingWriteSyncLock);
	PendingWriteList.AddTail(BlockPtr->PendingWriteNode);
	Notify();
}

void xWsServer::PostKillConnection(uint64_t ConnectionId) {
	auto BlockPtr = New(0);
	if (!BlockPtr) {
		Fatal("Not enough resource event to close a connection");
		return;
	}
	BlockPtr->ConnectionId = ConnectionId;

	auto G = xSpinlockGuard(PendingWriteSyncLock);
	PendingWriteList.AddTail(BlockPtr->PendingWriteNode);
	Notify();
}

void xWsServer::KillConnection(uint64_t ConnectionId) {
	assert(std::this_thread::get_id() == RunThreadId);
	auto PPss = ConnectionIdPool.CheckAndGet(ConnectionId);
	if (!PPss) {
		X_DEBUG_PRINTF("PPss missing: ConntectionId=%" PRIx64 "", ConnectionId);
		return;
	}
	auto Pss = (xPerSessionData *)PPss->P;
	if (Pss->Killing) {
		return;
	}
	Pss->Killing = true;
	lws_set_timeout(Pss->wsi, PENDING_TIMEOUT_USER_REASON_BASE, LWS_TO_KILL_ASYNC);
}

bool xWsServer::InitPss(struct xPerSessionData * Pss) {
	Construct(*Pss);
	Pss->ConnectionId = ConnectionIdPool.Acquire(xVariable{ .P = Pss });
	if (!Pss->ConnectionId) {
		return false;
	}
	PssList.AddTail(*Pss);
	return true;
}

void xWsServer::CleanPss(struct xPerSessionData * Pss) {
	assert(ConnectionIdPool.CheckAndGet(Pss->ConnectionId)->P == Pss);
	for (auto & N : Pss->SendBlockList) {
		auto BlockPtr = X_Entry(&N, xWsServer::xHttpSendBlock, PendingWriteNode);
		Delete(BlockPtr);
	}
	ConnectionIdPool.Release(Pss->ConnectionId);
	Destruct(*Pss);
}

xWsServer::xHttpSendBlock * xWsServer::New(size_t DataSize) {
	// allow DataSize as zero for special usage like closing a connection
	if (DataSize <= WsSendBlockBufferSize) {  // lock guard scope
		auto LockGuard = xSpinlockGuard(AllocationLock);
		auto IndexId   = SmallBlockBufferPool.Acquire();
		if (IndexId) {
			auto & Block  = SmallBlockBufferPool[IndexId].CreateAs<xHttpSendBlock>();
			Block.IndexId = IndexId;
			return &Block;
		}
	}
	auto BlockDummyPtr = (xHttpSendBlockDummy *)malloc(sizeof(xHttpSendBlockDummy) + DataSize - WsSendBlockBufferSize);
	if (X_UNLIKELY(!BlockDummyPtr)) {
		return nullptr;
	}
	Construct(*BlockDummyPtr);
	auto & Block = BlockDummyPtr->CreateAs<xHttpSendBlock>();
	return &Block;
}

void xWsServer::Delete(xWsServer::xHttpSendBlock * BlockPtr) {
	assert(std::this_thread::get_id() == RunThreadId);
	assert(BlockPtr);
	auto BlockDummyPtr = xHttpSendBlockDummy::CastPtr(BlockPtr);
	if (!BlockPtr->IndexId) {
		BlockDummyPtr->DestroyAs<xHttpSendBlock>();
		Destruct(*BlockDummyPtr);
		free(BlockPtr);
		return;
	}
	BlockDummyPtr->DestroyAs<xHttpSendBlock>();
	do {  // lock guard scope
		auto LockGuard = xSpinlockGuard(AllocationLock);
		assert(SmallBlockBufferPool.CheckAndGet(BlockPtr->IndexId) == BlockDummyPtr);
		SmallBlockBufferPool.Release(BlockPtr->IndexId);
	} while (false);
}
