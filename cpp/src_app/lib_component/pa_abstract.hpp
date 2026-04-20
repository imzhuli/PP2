#pragma once
#include <pp_common/_common.hpp>
#include <pp_common/device.hpp>

struct xPA_ClientConnection;

struct xProxyAbstractRelayConnection {
    uint64_t ConnectionId;

    virtual bool IsOpen() const      = 0;
    virtual bool IsConnected() const = 0;
    virtual bool IsClosing() const   = 0;
    virtual bool IsClosed() const    = 0;

    virtual void RequestCreateConnection(uint64_t DeviceId, const xNetAddress & RemoteAddress)                       = 0;
    virtual void RequestPostData(uint64_t DeviceId, uint64_t ConnectionId, const void * Payload, size_t PayloadSize) = 0;
    virtual void ProcessRelayResults()                                                                               = 0;
};
