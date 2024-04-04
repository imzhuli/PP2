#pragma once
#include "./protocol.hpp"

struct xHostQueryReq : xBinaryMessage {

	void SerializeMembers() override {
		W(Hostname);
	}

	void DeserializeMembers() override {
		R(Hostname);
	}

	std::string Hostname;
};

struct xHostQueryResp : xBinaryMessage {

	void SerializeMembers() override {
		W(Addr4, Addr6);
	}
	void DeserializeMembers() override {
		R(Addr4, Addr6);
	}

	xNetAddress Addr4;
	xNetAddress Addr6;
};
