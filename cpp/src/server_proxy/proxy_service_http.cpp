#include "./proxy_access.hpp"

#include <crypto/base64.hpp>

static const char AuthError[93] = "HTTP/1.1 407 Proxy Authentication Required\r\nProxy-Authenticate: Basic\r\nConnection: close\r\n\r\n";

size_t xProxyService::OnClientHttpInit(xProxyClientConnection * CCP, void * DataPtr, size_t DataSize) {
	CCP->State = CLIENT_STATE_HTTP_WAIT_FOR_HEADER;
	return OnClientHttpHeader(CCP, DataPtr, DataSize);
}

size_t xProxyService::OnClientHttpHeader(xProxyClientConnection * CCP, void * DataPtr, size_t DataSize) {
	auto & Unprocessed = CCP->Http.Unprocessed;
	Unprocessed.append((const char *)DataPtr, DataSize);
	size_t LineStart = 0;
	size_t LineEnd   = Unprocessed.find("\r\n");
	while (true) {
		if (LineEnd == Unprocessed.npos) {
			Unprocessed = Unprocessed.substr(LineStart);
			return DataSize;
		}
		if (LineEnd == LineStart) {  // end of header
			auto BodyStart = LineEnd + 2;
			if (BodyStart != Unprocessed.size()) {  // body read
				CCP->Http.Body = Unprocessed.substr(BodyStart);
			}
			if (!OnHttpHeaderDone(CCP)) {
				return InvalidDataSize;
			}
			return DataSize;
		}
		ProcessHttpHeaderLine(CCP, Unprocessed.substr(LineStart, LineEnd - LineStart));
		LineStart = LineEnd + 2;
		LineEnd   = Unprocessed.find("\r\n", LineStart);
	}
	X_PFATAL("Code should never reach here");
	return InvalidDataSize;
}

void xProxyService::ProcessHttpHeaderLine(xProxyClientConnection * CCP, const std::string & Line) {
	X_DEBUG_PRINTF("Line: \n%s", HexShow(Line).c_str());
	const char * CL = Line.c_str();

	if (CCP->Http.Mode == eHttpMode::UNSPEC) {  // reqeust line
		if (CL == strcasestr(CL, "CONNECT ")) {
			CCP->Http.Mode      = eHttpMode::RAW;
			auto TargetStartPtr = CL + size_t(8);
			auto TargetEndPtr   = strchr(TargetStartPtr, ' ');
			if (!TargetEndPtr) {
				X_DEBUG_PRINTF("Invalid target");
				return;
			}
			CCP->Http.TargetHost = std::string(TargetStartPtr, TargetEndPtr - TargetStartPtr);
			return;
		}

		CCP->Http.Mode = eHttpMode::NORMAL;
		// remove schema tags:
		auto Segs = Split(Line, " ");
		if (Segs.size() < 2) {
			return;
		}
		auto & Schema      = Segs[1];
		auto   TargetIndex = size_t(0);
		auto   TagIndex    = Schema.find("://");
		if (TagIndex != Schema.npos) {
			TargetIndex = TagIndex + 3;
		}
		// find target
		auto UriIndex = Schema.find("/", TargetIndex);
		if (UriIndex == Schema.npos) {
			X_DEBUG_PRINTF("Invalid path found");
			return;
		}
		CCP->Http.TargetHost = Schema.substr(TargetIndex, UriIndex - TargetIndex);
		Schema               = Schema.substr(UriIndex);

		auto SchemaLine = JoinStr(Segs, ' ');
		X_DEBUG_PRINTF("Update uri schema: [%s]", SchemaLine.c_str());
		CCP->Http.Header.append(SchemaLine);
		CCP->Http.Header.append("\r\n", 2);
		return;
	}
	if (CL == strcasestr(CL, "proxy-connection:")) {
		X_DEBUG_PRINTF("Remove Header: %s", CL);
		return;
	}
	if (CL == strcasestr(CL, "connection:")) {
		X_DEBUG_PRINTF("Remove Header: %s", CL);
		return;
	}
	if (CL == strcasestr(CL, "Proxy-Authorization:")) {
		auto Segs = Split(Line, " ");
		if (Segs.size() != 3) {  //
			X_DEBUG_PRINTF("Invalid authorization parts");
			return;
		}
		if (Segs[1] != "Basic") {
			X_DEBUG_PRINTF("Unsupported authorization method");
			return;
		}
		auto AP = Base64Decode(Segs[2].data(), Segs[2].size());
		if (AP.empty()) {
			X_DEBUG_PRINTF("Failed to parse authorization data");
			return;
		}
		X_DEBUG_PRINTF("Authorization: %s", AP.c_str());
		auto AuthSegs = Split(AP, ":");
		if (AuthSegs.size() != 2) {
			X_DEBUG_PRINTF("Invalid authorization segs");
			return;
		}
		CCP->Http.AccountName     = AuthSegs[0];
		CCP->Http.AccountPassword = AuthSegs[1];
		return;
	}

	CCP->Http.Header.append(Line);
	CCP->Http.Header.append("\r\n", 2);
}

bool xProxyService::OnHttpHeaderDone(xProxyClientConnection * CCP) {
	X_DEBUG_PRINTF("Header: \n%s", CCP->Http.Header.c_str());
	X_DEBUG_PRINTF("Overhead Header: (body):\n%s", HexShow(CCP->Http.Body).c_str());

	if (CCP->Http.Mode == eHttpMode::NORMAL) {
		CCP->Http.Header.append("Connection: close\r\n\r\n", 21);
	} else if (CCP->Http.Mode == eHttpMode::RAW) {
		Reset(CCP->Http.Header);
	} else {
		return false;
	}

	if (CCP->Http.TargetHost.empty()) {
		X_DEBUG_PRINTF("Missing target host");
		return false;
	}

	if (CCP->Http.AccountName.empty() && CCP->Http.AccountPassword.empty()) {
		X_DEBUG_PRINTF("Missing auth fields");
		CCP->State = CLIENT_STATE_CLOSED;
		CCP->PostData(AuthError, SafeLength(AuthError));
		FlushAndKillClientConnection(CCP);
		return true;
	}

	PostAuthRequest(CCP, CCP->Http.AccountName, CCP->Http.AccountPassword);
	CCP->State = CLIENT_STATE_HTTP_WAIT_FOR_AUTH_RESULT;
	return true;
}

void xProxyService::OnHttpAuthResult(xProxyClientConnection * CCP, const xProxyClientAuthResp & AuthResp) {
	if (!AuthResp.AuditKey) {
		X_DEBUG_PRINTF("No audit id found, close connection");
		CCP->State = CLIENT_STATE_CLOSED;
		CCP->PostData(AuthError, SafeLength(AuthError));
		FlushAndKillClientConnection(CCP);
		return;
	}
	CCP->TerminalControllerAddress = AuthResp.TerminalControllerAddress;
	CCP->TerminalId                = AuthResp.TerminalId;
	CCP->State                     = CLIENT_STATE_HTTP_AUTH_DONE;

	TryResolveHttpTargetHost(CCP);
	return;
}

void xProxyService::TryResolveHttpTargetHost(xProxyClientConnection * CCP) {
	assert(CCP->State == CLIENT_STATE_HTTP_AUTH_DONE);
	auto TryLocal = xNetAddress::Parse(CCP->Http.TargetHost);
	if (!TryLocal) {
		auto Segs = Split(CCP->Http.TargetHost, ":");
		if (Segs.empty()) {
			X_DEBUG_PRINTF("Missing host");
			KillClientConnection(CCP);
			return;
		}
		if (Segs.size() == 1) {
			CCP->TargetAddress = { .Port = 80 };
		} else {
			CCP->TargetAddress = { .Port = (uint16_t)atol(Segs[1].c_str()) };
		}
		CCP->State = CLIENT_STATE_HTTP_WAIT_FOR_DNS_RESULT;
		PostDnsRequest(CCP, Segs[0]);
		return;
	}

	DoHttpConnect(CCP);
	return;
}

void xProxyService::OnHttpDnsResult(xProxyClientConnection * CCP, const xHostQueryResp & DnsResp) {
	X_DEBUG_PRINTF("DnsResp.Addr4=%s", DnsResp.Addr4.ToString().c_str());
	if (!DnsResp.Addr4) {
		CCP->State = CLIENT_STATE_CLOSED;
		KillClientConnection(CCP);
		return;
	}
	auto Port               = CCP->TargetAddress.Port;
	CCP->TargetAddress      = DnsResp.Addr4;
	CCP->TargetAddress.Port = Port;
	DoHttpConnect(CCP);
}

void xProxyService::DoHttpConnect(xProxyClientConnection * CCP) {
	X_DEBUG_PRINTF("Trying to connect to %s", CCP->TargetAddress.ToString().c_str());
	auto RelayClientConnectionPtr = GetTerminalController(CCP);
	if (!RelayClientConnectionPtr) {
		X_DEBUG_PRINTF("Failed to establish connection to relay server");
		KillClientConnection(CCP);
		return;
	}
	CreateTargetConnection(RelayClientConnectionPtr, CCP);
	CCP->State = CLIENT_STATE_HTTP_TCP_CONNECTING;
}

size_t xProxyService::OnClientHttpBody(xProxyClientConnection * CCP, void * DataPtr, size_t DataSize) {
	CCP->Http.Body.append((const char *)DataPtr, DataSize);
	return DataSize;
}

void xProxyService::OnHttpConnected(xProxyClientConnection * CCP, uint64_t NewConnectionPairId) {
	if (!NewConnectionPairId) {
		X_DEBUG_PRINTF("Failed to establish http connection");
		KillClientConnection(CCP);
		return;
	}

	CCP->ConnectionPairId = NewConnectionPairId;
	CCP->State            = CLIENT_STATE_HTTP_TCP_ESTABLISHED;

	PostDataToTerminalController(CCP, CCP->Http.Header.data(), CCP->Http.Header.size());
	Reset(CCP->Http.Header);
	PostDataToTerminalController(CCP, CCP->Http.Body.data(), CCP->Http.Body.size());
	Reset(CCP->Http.Body);
	KeepAlive(CCP);

	if (CCP->Http.Mode == eHttpMode::RAW) {  // notify
		CCP->PostData("HTTP/1.1 200 Connection established\r\nProxy-agent: proxy / 1.0\r\n\r\n", 65);
	}
}

size_t xProxyService::OnClientHttpStream(xProxyClientConnection * CCP, void * DataPtr, size_t DataSize) {
	X_DEBUG_PRINTF("ClientHttpStream: size=%zi", DataSize);
	PostDataToTerminalController(CCP, DataPtr, DataSize);
	KeepAlive(CCP);
	return DataSize;
}
