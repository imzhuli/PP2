#include "./proxy_access.hpp"

void xProxyClientConnection::PostData(const void * DataPtr, size_t DataSize) {
	Profiler.MarkDownload(DataSize);
	xTcpConnection::PostData(DataPtr, DataSize);
}
