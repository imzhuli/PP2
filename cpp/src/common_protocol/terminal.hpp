#pragma once
#include "./protocol.hpp"

struct xCreateTerminalConnection : xBinaryMessage {

	void SerializeMembers() {
		W(SourceConnectionId, TargetAddress);
	}
	void DeserializeMembers() {
		R(SourceConnectionId, TargetAddress);
	}

	uint64_t    SourceConnectionId;
	xNetAddress TargetAddress;
};

struct xCreateTerminalConnectionResp : xBinaryMessage {
	void SerializeMembers() {
		W(SourceConnectionId, TerminalTargetConnectionId);
	}
	void DeserializeMembers() {
		R(SourceConnectionId, TerminalTargetConnectionId);
	}

	uint64_t SourceConnectionId;
	uint64_t TerminalTargetConnectionId;
};
