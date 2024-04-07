#pragma once
#include "./protocol.hpp"

static constexpr const size_t MaxRelayPayloadSize = 3600;

struct xCreateRelayConnectionPair : xBinaryMessage {
	void SerializeMembers() {
		W(ClientConnectionId, TerminalId, TargetAddress);
	}
	void DeserializeMembers() {
		R(ClientConnectionId, TerminalId, TargetAddress);
	}

	uint64_t    ClientConnectionId;
	uint64_t    TerminalId;
	xNetAddress TargetAddress;
};

struct xCreateRelayConnectionPairResp : xBinaryMessage {
	void SerializeMembers() {
		W(ClientConnectionId, ConnectionPairId);
	}
	void DeserializeMembers() {
		R(ClientConnectionId, ConnectionPairId);
	}

	uint64_t ClientConnectionId;
	uint64_t ConnectionPairId;
};

struct xCloseRelayConnectionPair : xBinaryMessage {
	void SerializeMembers() {
		W(ConnectionPairId);
	}
	void DeserializeMembers() {
		R(ConnectionPairId);
	}
	uint64_t ConnectionPairId;
};

struct xProxyToRelayData : xBinaryMessage {
	void SerializeMembers() {
		W(ConnectionPairId, DataView);
	}
	void DeserializeMembers() {
		R(ConnectionPairId, DataView);
	}

	uint64_t         ConnectionPairId;
	std::string_view DataView;
};

// struct xTerminalToRelayData : xBinaryMessage {
// 	static constexpr const size_t MaxPayloadSize = 3600;

// 	void SerializeMembers() {
// 		W(ConnectionPairId, DataView);
// 	}
// 	void DeserializeMembers() {
// 		R(ConnectionPairId, DataView);
// 	}

// 	uint64_t         ConnectionPairId;
// 	std::string_view DataView;
// };

// struct xProxyAccessToRelayData : xBinaryMessage {
// 	static constexpr const size_t MaxPayloadSize = 3600;

// 	void SerializeMembers() {
// 		W(ClientConnectionId, DataView);
// 	}
// 	void DeserializeMembers() {
// 		R(ClientConnectionId, DataView);
// 	}

// 	uint64_t         ClientConnectionId;
// 	std::string_view DataView;
// };
