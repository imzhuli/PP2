#pragma once
#include "./protocol.hpp"

static constexpr const size_t MaxRelayPayloadSize = 4096;

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

struct xRelayToProxyData : xBinaryMessage {
	void SerializeMembers() {
		W(ClientConnectionId, DataView);
	}
	void DeserializeMembers() {
		R(ClientConnectionId, DataView);
	}

	uint64_t         ClientConnectionId;
	std::string_view DataView;
};

struct xCloseClientConnection : xBinaryMessage {
	void SerializeMembers() {
		W(ClientConnectionId);
	}
	void DeserializeMembers() {
		R(ClientConnectionId);
	}

	uint64_t ClientConnectionId;
};

struct xCreateUdpAssociation : xBinaryMessage {
	void SerializeMembers() {
		W(ClientConnectionId, TerminalId);
	}
	void DeserializeMembers() {
		R(ClientConnectionId, TerminalId);
	}

	uint64_t ClientConnectionId;
	uint64_t TerminalId;
};

struct xCreateUdpAssociationResp : xBinaryMessage {
	void SerializeMembers() {
		W(ClientConnectionId, ConnectionPairId, BindAddress);
	}
	void DeserializeMembers() {
		R(ClientConnectionId, ConnectionPairId, BindAddress);
	}

	uint64_t    ClientConnectionId;
	uint64_t    ConnectionPairId;
	xNetAddress BindAddress;
};

struct xCloseUdpAssociation : xBinaryMessage {
	void SerializeMembers() {
		W(ConnectionPairId);
	}
	void DeserializeMembers() {
		R(ConnectionPairId);
	}
	uint64_t ConnectionPairId;
};

struct xProxyToRelayUdpData : xBinaryMessage {
	void SerializeMembers() {
		W(ConnectionPairId, ToAddress, DataView);
	}
	void DeserializeMembers() {
		R(ConnectionPairId, ToAddress, DataView);
	}

	uint64_t         ConnectionPairId;
	xNetAddress      ToAddress;
	std::string_view DataView;
};

struct xRelayToProxyUdpData : xBinaryMessage {
	void SerializeMembers() {
		W(ClientConnectionId, FromAddress, DataView);
	}
	void DeserializeMembers() {
		R(ClientConnectionId, FromAddress, DataView);
	}

	uint64_t         ClientConnectionId;
	xNetAddress      FromAddress;
	std::string_view DataView;
};