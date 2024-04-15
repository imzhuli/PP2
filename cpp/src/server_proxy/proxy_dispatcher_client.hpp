#pragma once
#include "../common/base.hpp"

class xProxyService;

class xProxyDispatcherClient : public xClient {
public:
	bool Init(xIoContext * IoContextPtr, const xNetAddress & TargetAddress, xProxyService * ProxyServicePtr) {
		RuntimeAssert(xClient::Init(IoContextPtr, TargetAddress));
		this->ProxyServicePtr = ProxyServicePtr;
		return true;
	}
	void Clean() {
		xClient::Clean();
		Reset(ProxyServicePtr);
	}

protected:
	bool OnPacket(const xPacketHeader & Header, ubyte * PayloadPtr, size_t PayloadSize) override;

protected:
	xProxyService * ProxyServicePtr = nullptr;
};