#include "../lib_component/pa_service.hpp"

struct xPA_ClientTimeoutNode : xListNode {
    uint64_t TimestampMS;
};
using xPA_TimeoutList = xList<xPA_ClientTimeoutNode>;

struct xPA_ClientConnection : xTcpConnection {
};

class xPA_LocalBindingService : xPA_ServiceBase
    , xTcpServer::iListener {
public:
    bool Init(const std::string & JsonConfigFile);
    void Clean();

private:
    bool LoadConfig(const std::string & JsonConfigFile);

    // override:
    void OnNewConnection(xTcpServer * TcpServerPtr, xSocket && NativeHandle) override;

private:
    xTcpServer TcpServer;

    xPA_TimeoutList AuthTimeoutList;
    xPA_TimeoutList AcquireChannelTimeoutList;

    xNetAddress BindAddress;
};
