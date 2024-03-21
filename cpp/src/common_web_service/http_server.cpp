#include "./http_server.hpp"

#include <libwebsockets.h>

#include <core/core_value_util.hpp>

// defaults:
static constexpr int    LWS_SHOULD_CLOSE     = 1;
static constexpr size_t HttpHeaderBufferSize = 1'000;

// libwebsockets callbacs:
struct xPerSessionData : xHttpServer::xPssListNode {
	struct lws *                    wsi            = nullptr;
	bool                            Writing        = false;
	bool                            WritingHeaders = false;
	bool                            Killing        = false;
	uint64_t                        ConnectionId   = 0;
	xHttpServer::xHttpSendBlockList SendBlockList;
	xNetAddress                     PeerAddress;
	std::string                     Uri;
	xHttpHeaderList                 RequestHeaders;
	std::string                     Body;
	ubyte                           PreWriteBuffer[LWS_PRE];
	ubyte                           WriteHeaderBuffer[HttpHeaderBufferSize];
};

const std::string xHttpRequest::EmptyHeaderValue = {};
xHttpRequest::xHttpRequest(xPerSessionData * RealPss)
	: Headers(RealPss->RequestHeaders), Uri(RealPss->Uri), Body(RealPss->Body) {
	wsi.P = RealPss->wsi;
}

bool xHttpRequest::IsMethod(eHttpMethod Method) const {
	auto real_wsi = (lws *)wsi.P;
	switch (Method) {
		case eHttpMethod::GET:
			return lws_hdr_total_length(real_wsi, WSI_TOKEN_GET_URI);
		case eHttpMethod::POST:
			return lws_hdr_total_length(real_wsi, WSI_TOKEN_POST_URI);
		case eHttpMethod::OPTIONS:
			return lws_hdr_total_length(real_wsi, WSI_TOKEN_OPTIONS_URI);
		case eHttpMethod::PUT:
			return lws_hdr_total_length(real_wsi, WSI_TOKEN_PUT_URI);
		case eHttpMethod::DELETE:
			return lws_hdr_total_length(real_wsi, WSI_TOKEN_DELETE_URI);
		default:
			break;
	}
	return Method == eHttpMethod::OTHER;
}

class xHttpServerDelegate {
public:
	static_assert(sizeof(xHttpServer::xHttpSendBlock::ReservedForPRE) >= LWS_PRE);

	static int  LwsCallback(struct lws * wsi, enum lws_callback_reasons reason, void * session, void * in, size_t len);
	static void LwsHttpHeaderForEachCallback(const char * name, int nlen, void * opaque);
};

void xHttpServerDelegate::LwsHttpHeaderForEachCallback(const char * name, int nlen, void * opaque) {
	X_DEBUG_PRINTF("name=%s", name);
}

int xHttpServerDelegate::LwsCallback(struct lws * wsi, enum lws_callback_reasons reason, void * session, void * in, size_t len) {
	auto ContextPtr = lws_get_context(wsi);
	auto ServerPtr  = (xHttpServer *)lws_context_user(ContextPtr);
	auto Pss        = (xPerSessionData *)session;
	switch (reason) {

		case LWS_CALLBACK_HTTP: {
			// X_DEBUG_PRINTF("LWS_CALLBACK_HTTP");
			if (!ServerPtr->InitPss(Pss) || !in || !len) {
				lws_return_http_status(wsi, 404, nullptr);
				return LWS_SHOULD_CLOSE;
			}
			Pss->wsi = wsi;
			Pss->Uri = std::string((const char *)in, len);

			char AddressNameBuffer[1024];
			char AddressIpBuffer[128];
			auto socket_fd = lws_get_socket_fd(wsi);
			lws_get_peer_addresses(wsi, socket_fd, AddressNameBuffer, sizeof(AddressNameBuffer), AddressIpBuffer, sizeof(AddressIpBuffer));
			Pss->PeerAddress = xNetAddress::Parse(AddressIpBuffer);
			// X_DEBUG_PRINTF("NewConnection: Name=%s,Ip=%s, PeerAddreess=%s", AddressNameBuffer, AddressIpBuffer, Pss->PeerAddress.IpToString().c_str());

			if (lws_hdr_total_length(wsi, WSI_TOKEN_OPTIONS_URI)) {
				auto P = Pss->WriteHeaderBuffer;
				auto E = P + sizeof(Pss->WriteHeaderBuffer);
				if (lws_add_http_common_headers(wsi, 200, "text/plain", 0, &P, E) ||
					lws_add_http_header_by_name(wsi, (const ubyte *)"Access-Control-Allow-Origin", (const ubyte *)"*", 1, &P, E) ||
					lws_add_http_header_by_name(wsi, (const ubyte *)"Access-Control-Allow-Headers", (const ubyte *)"*", 1, &P, E) ||
					lws_add_http_header_by_name(wsi, (const ubyte *)"Access-Control-Allow-Methods", (const ubyte *)"GET, POST", 9, &P, E) ||
					lws_add_http_header_by_name(wsi, (const ubyte *)"Connection", (const ubyte *)"close", 5, &P, E) ||
					lws_finalize_write_http_header(wsi, Pss->WriteHeaderBuffer, &P, E)) {
					return LWS_SHOULD_CLOSE;
				}
				return 0;
			}

			size_t ContentLength = 0;
			do {  // content-length
				char Buffer[64];
				if (0 < lws_hdr_copy(wsi, Buffer, sizeof(Buffer), WSI_TOKEN_HTTP_CONTENT_LENGTH)) {
					ContentLength = (size_t)std::strtoumax(Buffer, nullptr, 10);
				}
				ContentLength = (size_t)std::strtoumax(Buffer, nullptr, 10);
			} while (false);

			auto InterestedHeaders = ServerPtr->GetInterestedHeaders();
			for (auto & N : InterestedHeaders) {
				auto HeaderName        = N + ':';
				auto HeaderValue       = std::string();
				auto HeaderValueLength = lws_hdr_custom_length(wsi, HeaderName.data(), (int)HeaderName.length());
				if (HeaderValueLength != -1) {
					HeaderValue.resize(HeaderValueLength);
					lws_hdr_custom_copy(wsi, HeaderValue.data(), HeaderValue.size() + 1, HeaderName.data(), (int)HeaderName.length());
				}
				Pss->RequestHeaders.push_back(std::move(HeaderValue));
			}
			if (!ContentLength) {
				return ServerPtr->OnHttpRequest(Pss->ConnectionId, xHttpRequest(Pss)) ? 0 : LWS_SHOULD_CLOSE;
			}
			return 0;
		}

		case LWS_CALLBACK_HTTP_BODY: {
			assert(in && len);
			Pss->Body.append((const char *)in, len);
			return 0;
		}

		case LWS_CALLBACK_HTTP_BODY_COMPLETION: {
			// X_DEBUG_PRINTF("LWS_CALLBACK_HTTP_BODY_COMPLETION CID=%" PRIx64 ", uri=%s body=%s", Pss->ConnectionId, Pss->Uri.c_str(), Pss->Body.c_str());
			return ServerPtr->OnHttpRequest(Pss->ConnectionId, xHttpRequest(Pss)) ? 0 : LWS_SHOULD_CLOSE;
		}

		case LWS_CALLBACK_CLOSED_HTTP:
			// X_DEBUG_PRINTF("LWS_CALLBACK_CLOSED_HTTP");
			if (!Pss || !Pss->wsi) {  // initialization not completed
				break;
			}
			ServerPtr->CleanPss(Pss);
			break;

		case LWS_CALLBACK_HTTP_WRITEABLE: {
			// X_DEBUG_PRINTF("LWS_CALLBACK_HTTP_WRITEABLE: CID=%" PRIx64 "", Pss->ConnectionId);
			auto NodePtr = Pss->SendBlockList.Head();
			if (!NodePtr) {
				assert(!Pss->Writing);
				break;
			}

			auto BlockPtr = X_Entry(NodePtr, xHttpServer::xHttpSendBlock, PendingWriteNode);
			if (Steal(Pss->WritingHeaders)) {  // case 1: continue to write body
				assert(Pss->Writing);
				if (BlockPtr->DataSize) {
					if (lws_write(wsi, BlockPtr->BodyBuffer, BlockPtr->DataSize, LWS_WRITE_HTTP) < (int)BlockPtr->DataSize) {
						return LWS_SHOULD_CLOSE;
					}
				}
				if (BlockPtr->ShouldClose) {
					return LWS_SHOULD_CLOSE;
				}
				lws_callback_on_writable(wsi);
				return 0;
			}

			if (Pss->Writing) {  // case 2: old block finished
				ServerPtr->Delete(BlockPtr);
				NodePtr = Pss->SendBlockList.Head();
			}
			if (!(Pss->Writing = NodePtr)) {
				break;
			}

			// case 2.1: write new block header
			BlockPtr = X_Entry(NodePtr, xHttpServer::xHttpSendBlock, PendingWriteNode);
			auto P   = Pss->WriteHeaderBuffer;
			auto E   = P + sizeof(Pss->WriteHeaderBuffer);
			if (lws_add_http_common_headers(wsi, 200, BlockPtr->ContentType, (lws_filepos_t)BlockPtr->DataSize, &P, E) ||
				lws_add_http_header_by_name(wsi, (const ubyte *)"Access-Control-Allow-Origin", (const ubyte *)"*", 1, &P, E) ||
				lws_add_http_header_by_name(wsi, (const ubyte *)"Access-Control-Allow-Headers", (const ubyte *)"*", 1, &P, E) ||
				lws_add_http_header_by_name(wsi, (const ubyte *)"Access-Control-Allow-Methods", (const ubyte *)"GET, POST", 9, &P, E)) {
				return LWS_SHOULD_CLOSE;
			}
			if (BlockPtr->ShouldClose && lws_add_http_header_by_name(wsi, (const ubyte *)"Connection", (const ubyte *)"close", 5, &P, E)) {
				return LWS_SHOULD_CLOSE;
			}
			if (lws_finalize_write_http_header(wsi, Pss->WriteHeaderBuffer, &P, E)) {
				return LWS_SHOULD_CLOSE;
			}
			Pss->WritingHeaders = true;
			lws_callback_on_writable(wsi);
			return 0;
		}  // clang-format off
			
		// CASE_PRINT(LWS_CALLBACK_GET_THREAD_ID);
		// CASE_PRINT(LWS_CALLBACK_PROTOCOL_INIT);
		// CASE_PRINT(LWS_CALLBACK_WSI_CREATE);
		// CASE_PRINT(LWS_CALLBACK_WSI_DESTROY);
		// CASE_PRINT(LWS_CALLBACK_SERVER_NEW_CLIENT_INSTANTIATED);
		// CASE_PRINT(LWS_CALLBACK_HTTP_BIND_PROTOCOL);
		// CASE_PRINT(LWS_CALLBACK_EVENT_WAIT_CANCELLED);

		// CASE_PRINT(LWS_CALLBACK_FILTER_HTTP_CONNECTION);
		// CASE_PRINT(LWS_CALLBACK_HTTP_DROP_PROTOCOL);
		// CASE_PRINT(LWS_CALLBACK_FILTER_NETWORK_CONNECTION);

		default:
			// X_DEBUG_PRINTF("Uncaught callback %i, PSS=%p", (int)reason, Pss);
			break;
	}
	return 0;
}

[[maybe_unused]] static struct lws_protocols protocols[] = {
	{ "http", xHttpServerDelegate::LwsCallback, sizeof(xPerSessionData), MaxPacketSize, 0, nullptr, 0 },
	LWS_PROTOCOL_LIST_TERM,
};

////////////// Server //////////////

bool xHttpServer::Init(const xHttpServerOptions & Options) {

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

void xHttpServer::Clean() {
	assert(PssList.IsEmpty());
	lws_context_destroy((struct lws_context *)Steal(LwsContext.P));
	SmallBlockBufferPool.Clean();
	ConnectionIdPool.Clean();
}

void xHttpServer::Run() {
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
			auto PPss     = ConnectionIdPool.CheckAndGet(BlockPtr->ConnectionId);
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

void xHttpServer::Notify() {
	auto context = (struct lws_context *)LwsContext.P;
	assert(context);
	lws_cancel_service(context);
}

void xHttpServer::Stop() {
	RunState.Stop();
	Notify();
}

void xHttpServer::PostData(uint64_t ConnectionId, const char * ContentType, const void * DataPtr, size_t DataSize, bool ShouldClose) {
	auto BlockPtr = New(DataSize);
	if (!BlockPtr) {
		return;
	}
	BlockPtr->ConnectionId = ConnectionId;
	BlockPtr->ContentType  = ContentType;
	BlockPtr->DataSize     = DataSize;
	BlockPtr->ShouldClose  = ShouldClose;
	memcpy(BlockPtr->BodyBuffer, DataPtr, DataSize);

	auto G = xSpinlockGuard(PendingWriteSyncLock);
	PendingWriteList.AddTail(BlockPtr->PendingWriteNode);
	Notify();
}

void xHttpServer::PostKillConnection(uint64_t ConnectionId) {
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

void xHttpServer::KillConnection(uint64_t ConnectionId) {
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

bool xHttpServer::InitPss(struct xPerSessionData * Pss) {
	Construct(*Pss);
	Pss->ConnectionId = ConnectionIdPool.Acquire(xVariable{ .P = Pss });
	if (!Pss->ConnectionId) {
		return false;
	}
	// X_DEBUG_PRINTF("Pss=%p, ConnectionId=%" PRIx64 "", Pss, Pss->ConnectionId);
	PssList.AddTail(*Pss);
	return true;
}

void xHttpServer::CleanPss(struct xPerSessionData * Pss) {
	assert(ConnectionIdPool.CheckAndGet(Pss->ConnectionId)->P == Pss);
	// X_DEBUG_PRINTF("Pss=%p, ConnectionId=%" PRIx64 "", Pss, Pss->ConnectionId);
	for (auto & N : Pss->SendBlockList) {
		auto BlockPtr = X_Entry(&N, xHttpServer::xHttpSendBlock, PendingWriteNode);
		Delete(BlockPtr);
	}
	ConnectionIdPool.Release(Pss->ConnectionId);
	Destruct(*Pss);
}

xHttpServer::xHttpSendBlock * xHttpServer::New(size_t DataSize) {
	// allow DataSize as zero for special usage like closing a connection
	if (DataSize <= HttpBodyBufferSize) {  // lock guard scope
		auto LockGuard = xSpinlockGuard(AllocationLock);
		auto IndexId   = SmallBlockBufferPool.Acquire();
		if (IndexId) {
			auto & Block  = SmallBlockBufferPool[IndexId].CreateAs<xHttpSendBlock>();
			Block.IndexId = IndexId;
			return &Block;
		}
	}
	auto BlockDummyPtr = (xHttpSendBlockDummy *)malloc(sizeof(xHttpSendBlockDummy) + DataSize - HttpBodyBufferSize);
	if (X_UNLIKELY(!BlockDummyPtr)) {
		return nullptr;
	}
	Construct(*BlockDummyPtr);
	auto & Block = BlockDummyPtr->CreateAs<xHttpSendBlock>();
	return &Block;
}

void xHttpServer::Delete(xHttpServer::xHttpSendBlock * BlockPtr) {
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
