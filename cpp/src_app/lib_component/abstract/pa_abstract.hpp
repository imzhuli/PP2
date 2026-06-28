#pragma once
#include <pp_common/_common.hpp>
#include <pp_common/device.hpp>

struct xPA_ClientConnection;
struct xPA_ClientUdpChannel;

struct xProxyAbstractService {
    virtual void PostData(uint64_t ProxyClientConnectionId, const void * DataPtr, size_t DataSize)                                    = 0;
    virtual void PostData(uint64_t ProxyClientUdpChannelId, const xNetAddress & SourceAddress, const void * DataPtr, size_t DataSize) = 0;
    virtual void CloseConnection(uint64_t ProxyClientConnectionId)                                                                    = 0;
    virtual void UpdateConsumedTcpDataSizeByTarget(uint64_t ProxyClientConnectionId, size_t ConsumedSize)                             = 0;
};
