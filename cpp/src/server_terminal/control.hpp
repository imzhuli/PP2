#pragma once
#include "../common/base.hpp"

class xControlService : public xClient {
public:
	using xClient::Clean;
	using xClient::Init;

	void Tick(uint64_t NowMS);

private:
	void OnServerConnected() override;
	void OnServerClose() override;
	bool OnPacket(const xPacketHeader & Header, ubyte * PayloadPtr, size_t PayloadSize) override;
};
