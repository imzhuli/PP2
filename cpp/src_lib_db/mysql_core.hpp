#pragma once
#include "./common.hpp"

bool IsMySqlServerLostError(void * MySql);

class xMySqlConn final {
public:
	bool Init(const std::string & user, const std::string & password, const std::string & database, const std::string & host, uint16_t port);
	void Clean();

	void * GetNative() const {
		if (!IsReady()) {
			return nullptr;
		}
		return NativeHandle.Get();
	}

	void SetDisconnected() {
		Disconnected = true;
	}

	bool IsReady() const {
		if (!Disconnected) {
			assert(NativeHandle.IsEnabled());
			return true;
		}
		return false;
	}

	xVersionNumber GetVersion() const {
		return Disconnected ? xVersionNumber() : NativeHandle.GetVersion();
	}

	void Open();
	void Close();
	bool CheckReconnection();
	void Tick() {
		Tick(GetTimestampMS());
	}
	void Tick(uint64_t NowMS);

private:
	// config
	std::string User;
	std::string Password;
	std::string Database;
	std::string Host;
	uint16_t    Port;

	// run state
	bool             Disconnected = true;
	xVersion<void *> NativeHandle = {};

	// check
	uint64_t NowMS           = 0;
	uint64_t LastTimestampMS = 0;
};
