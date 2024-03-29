#pragma once
#include "../common/base.hpp"

class xChallenge : public xBinaryMessage {
public:
	void SerializeMembers() override {
		W(CountryId);
		W(CityId);
		W(HW);
	}
	void DeserializeMembers() override {
		R(CountryId);
		R(CityId);
		R(HW);
	}

public:
	uint32_t CountryId;
	uint32_t CityId;
	string   HW = "Hello World!";
};

class xChallengeResp : public xBinaryMessage {
public:
	void SerializeMembers() override {
		W(ServerAddress);
		W(ChallengeKey);
		W(TerminalId);
	}
	void DeserializeMembers() override {
		R(ServerAddress);
		R(ChallengeKey);
		R(TerminalId);
	}

public:
	std::string ServerAddress;
	std::string ChallengeKey;
	uint64_t    TerminalId;
};
