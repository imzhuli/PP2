#pragma once
#include "./pa_abstract.hpp"

class xProxyClientConnection final : public xProxyAbastractClientConnection {
};

class xProxyLocalRelayConnection final : public xProxyAbstractRelayConnection {
    bool IsOpen() const override {
        return true;
    };
    bool IsConnected() const override {
        return true;
    }
    bool IsClosing() const override {
        return false;
    }
    bool IsClosed() const override {
        return false;
    }

    void RequestCreateConnection(uint64_t DeviceId, const xNetAddress & RemoteAddress) override;
    void RequestPostData(uint64_t DeviceId, uint64_t ConnectionId, const void * Payload, size_t PayloadSize) override;
    void ProcessRelayResults() override;
};

class xProxyLocalRelayService final {
public:
private:
    xIndexedStorage<xProxyLocalRelayConnection *> LocalRelayPool;
};