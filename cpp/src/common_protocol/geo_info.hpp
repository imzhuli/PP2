#pragma once
#include "./protocol.hpp"

struct xGeoInfoReq : xBinaryMessage {
	void SerializeMembers() override {
		W(IP, HelloWorld);
	}
	void DeserializeMembers() override {
		R(IP, HelloWorld);
	}

	std::string IP;
	std::string HelloWorld;
};

struct xGeoInfoResp : xBinaryMessage {
	void SerializeMembers() override {
		W(IP, CountryId, CityId, CityName, Signature);
	}
	void DeserializeMembers() override {
		R(IP, CountryId, CityId, CityName, Signature);
	}

	std::string IP;
	uint32_t    CountryId;
	uint32_t    CityId;
	std::string CityName;
	std::string Signature;
};
