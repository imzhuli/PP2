#pragma once
#include "./protocol.hpp"

struct xCreateTerminalConnection : xBinaryMessage {
	void SerializeMembers() {
		W(ClientConnectionId, TermainlId, TargetAddress);
	}
	void DeserializeMembers() {
		R(ClientConnectionId, TermainlId, TargetAddress);
	}

	uint64_t    ClientConnectionId;
	uint64_t    TermainlId;
	xNetAddress TargetAddress;
};

struct xCreateTerminalConnectionResp : xBinaryMessage {
	void SerializeMembers() {
		W(ClientConnectionId, TerminalTargetConnectionId);
	}
	void DeserializeMembers() {
		R(ClientConnectionId, TerminalTargetConnectionId);
	}

	uint64_t ClientConnectionId;
	uint64_t TerminalTargetConnectionId;
};

struct xCloseTerminalConnection : xBinaryMessage {
	void SerializeMembers() {
		W(TerminalId, TerminalTargetConnectionId);
	}
	void DeserializeMembers() {
		R(TerminalId, TerminalTargetConnectionId);
	}

	uint64_t TerminalId;
	uint64_t TerminalTargetConnectionId;
};

struct xRelayToTerminalData : xBinaryMessage {
	static constexpr const size_t MaxPayloadSize = 3600;

	void SerializeMembers() {
		W(TerminalTargetConnectionId, TerminalTargetConnectionId, DataView);
	}
	void DeserializeMembers() {
		R(TerminalTargetConnectionId, TerminalTargetConnectionId, DataView);
	}

	uint64_t         TerminalTargetConnectionId;
	std::string_view DataView;
};

struct xTerminalToRelayData : xBinaryMessage {
	static constexpr const size_t MaxPayloadSize = 3600;

	void SerializeMembers() {
		W(RelayConnectionPairId, DataView);
	}
	void DeserializeMembers() {
		R(RelayConnectionPairId, DataView);
	}

	uint64_t         RelayConnectionPairId;
	std::string_view DataView;
};

struct xProxyAccessToRelayData : xBinaryMessage {
	static constexpr const size_t MaxPayloadSize = 3600;

	void SerializeMembers() {
		W(ClientConnectionId, DataView);
	}
	void DeserializeMembers() {
		R(ClientConnectionId, DataView);
	}

	uint64_t         ClientConnectionId;
	std::string_view DataView;
};
