#pragma once
#include "./protocol.hpp"

struct xProxyClientAuth : xBinaryMessage {
	void SerializeMembers() {
		W(AddressString, AccountName, Password);
	}
	void DeserializeMembers() {
		R(AddressString, AccountName, Password);
	}
	std::string AddressString;
	std::string AccountName;
	std::string Password;
};

struct xProxyClientAuthResp : xBinaryMessage {
	void SerializeMembers() {
		W(AuditKey, CacheTimeout);
		W(TerminalControllerAddress, TerminalId);
		W(EnableUdp);
	}
	void DeserializeMembers() {
		R(AuditKey, CacheTimeout);
		R(TerminalControllerAddress, TerminalId);
		R(EnableUdp);
	}
	uint64_t    AuditKey;                   // 绑定的计量账号(不是计费)
	uint64_t    CacheTimeout;               // 校验超时, 单位:毫秒, 一般用户采用默认值, 特别用户使用设置的较短值
	xNetAddress TerminalControllerAddress;  // relay server, or terminal service address
	uint64_t    TerminalId;                 // index in relay server
	bool        EnableUdp;
};
