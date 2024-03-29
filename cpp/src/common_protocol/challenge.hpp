#pragma once
#include "../common/base.hpp"

class xChallenge : public xBinaryMessage {
public:
	void SerializeMembers() override {
		W(CountryId, CityId, HW);
	}
	void DeserializeMembers() override {
		R(CountryId, CityId, HW);
	}

public:
	uint32_t CountryId;
	uint32_t CityId;
	string   HW = "Hello World!";
};

class xChallengeResp : public xBinaryMessage {
public:
	void SerializeMembers() override {
		W(ServerAddress, ChallengeKey, TerminalId);
	}
	void DeserializeMembers() override {
		R(ServerAddress, ChallengeKey, TerminalId);
	}

public:
	std::string ServerAddress;
	std::string ChallengeKey;
	uint64_t    TerminalId;
};
