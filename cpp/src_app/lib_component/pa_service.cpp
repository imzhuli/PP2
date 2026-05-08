#include "./pa_service.hpp"

#include <pp_common/service_runtime.hpp>

static constexpr const size_t   PA_CLIENT_AUTH_TIMEOUT_MS        = 2'000;
static constexpr const uint64_t PA_FUTURE_TIMEOUT_MS             = 1'000;
static constexpr const size_t   PA_AUDIT_TIMEOUT_MS              = 5'000;
static constexpr const size_t   PA_MAX_CLIENT_CONNECTION         = 20'0000;
static constexpr const size_t   PA_MAX_CLIENT_REQUEST_PER_SECOND = 5'0000;

static ubyte S5_CONNECTION_IPV4_FAILED[] = {
    '\x05',                          // version
    '\x01',                          // generic error
    '\x00',                          // reserved
    '\x01',                          // type=ipv4
    '\x00', '\x00', '\x00', '\x00',  // addr.ipv4
    '\x00', '\x00',                  // port
};
static ubyte S5_CONNECTION_IPV6_FAILED[] = {
    '\x05',                          // version
    '\x01',                          // generic error
    '\x00',                          // reserved
    '\x04',                          // type=ipv6
    '\x00', '\x00', '\x00', '\x00',  // addr.ipv6
    '\x00', '\x00', '\x00', '\x00',  // addr.ipv6
    '\x00', '\x00', '\x00', '\x00',  // addr.ipv6
    '\x00', '\x00', '\x00', '\x00',  // addr.ipv6
    '\x00', '\x00',                  // port
};

bool xProxyAccessService::Init(const xNetAddress & BindAddress4, const xNetAddress & BindAddress6) {
    X_RUNTIME_ASSERT(ServiceRunState);
    if (!ClientConnectionPool.Init(PA_MAX_CLIENT_CONNECTION)) {
        DEBUG_LOG("ClientConnectionPool init error");
        return false;
    }
    auto ClientConnectionPoolCleaner = xScopeCleaner(ClientConnectionPool);

    if (!ClientUdpChannelPool.Init(PA_MAX_CLIENT_CONNECTION)) {
        DEBUG_LOG("ClientUdpChannelPool init error");
        return false;
    }
    auto ClientUdpChannelPoolCleaner = xScopeCleaner(ClientUdpChannelPool);

    X_RUNTIME_ASSERT(BindAddress4 || BindAddress6);
    auto TcpServerCleaner = xScopeGuard([this] {
        if (TcpServer4) {
            TcpServer4->Clean();
            delete Steal(TcpServer4);
        }
        if (TcpServer6) {
            TcpServer6->Clean();
            delete Steal(TcpServer6);
        }
    });
    if (BindAddress4) {
        TcpServer4 = new xTcpServer();
        if (!TcpServer4->Init(ServiceIoContext, BindAddress4, this)) {
            DEBUG_LOG("TcpServer4 init error");
            delete Steal(TcpServer4);
            return false;
        }
    }
    if (BindAddress6) {
        TcpServer6 = new xTcpServer();
        if (!TcpServer6->Init(ServiceIoContext, BindAddress6, this)) {
            DEBUG_LOG("TcpServer4 init error");
            delete Steal(TcpServer6);
            return false;
        }
    }

    if (!AuthFutureManager.Init(PA_MAX_CLIENT_REQUEST_PER_SECOND)) {
        DEBUG_LOG("AuthFutureManager init error");
        return false;
    }
    auto AuthFutureManagerCleaner = xScopeCleaner(AuthFutureManager);

    if (!AcquireDeviceFutureManager.Init(PA_MAX_CLIENT_REQUEST_PER_SECOND)) {
        DEBUG_LOG("AcquireDeviceFutureManager init error");
        return false;
    }
    auto AcquireDeviceFutureManagerCleaner = xScopeCleaner(AcquireDeviceFutureManager);

    if (!AcquireDeviceConnectionFutureManager.Init(PA_MAX_CLIENT_REQUEST_PER_SECOND)) {
        DEBUG_LOG("AcquireDeviceConnectionFutureManager init error");
        return false;
    }
    auto AcquireDeviceConnectionFutureManagerCleaner = xScopeCleaner(AcquireDeviceConnectionFutureManager);

    if (!AcquireDeviceUdpChannelFutureManager.Init(PA_MAX_CLIENT_REQUEST_PER_SECOND)) {
        DEBUG_LOG("AcquireDeviceUdpChannelFutureManager init error");
        return false;
    }
    auto AcquireDeviceUdpChannelFutureManagerCleaner = xScopeCleaner(AcquireDeviceUdpChannelFutureManager);

    ClientConnectionPoolCleaner.Dismiss();
    TcpServerCleaner.Dismiss();
    AuthFutureManagerCleaner.Dismiss();
    AcquireDeviceConnectionFutureManagerCleaner.Dismiss();
    AcquireDeviceUdpChannelFutureManagerCleaner.Dismiss();

    Reset(BindUdpAddress4);
    Reset(BindUdpAddress6);
    Reset(ExportUdpAddress4);
    Reset(ExportUdpAddress6);

    return true;
}

void xProxyAccessService::Clean() {
    AuthFutureManager.Clean();
    AcquireDeviceFutureManager.Clean();
    AcquireDeviceConnectionFutureManager.Clean();
    AcquireDeviceUdpChannelFutureManager.Clean();
    do {  // clean tcp servers
        if (TcpServer4) {
            TcpServer4->Clean();
            delete Steal(TcpServer4);
        }
        if (TcpServer6) {
            TcpServer6->Clean();
            delete Steal(TcpServer6);
        }
    } while (false);
    Reset(BindUdpAddress4);
    Reset(BindUdpAddress6);
    Reset(ExportUdpAddress4);
    Reset(ExportUdpAddress6);
    ClientUdpChannelPool.Clean();
    ClientConnectionPool.Clean();

    Reset(Audit);
}

void xProxyAccessService::Tick(uint64_t NowMS) {
    LocalTicker.Update(NowMS);
    // process auth results:
    ProcessReadyAuthFuture();
    ProcessReadyAcquireDeviceFuture();
    ProcessReadyAcquireDeviceConnectionFuture();
    ProcessReadyAcquireDeviceUdpChannelFuture();

    DeferKillInitTimeoutConnection();
    ExcuteKillConnection();
    ClearTimeoutFuture();
}

void xProxyAccessService::ProcessReadyAuthFuture() {
    auto & FutureList = AuthFutureManager.GetReadyFutureList();
    while (auto P = static_cast<xPA_AuthFuture *>(FutureList.PopHead())) {
        DEBUG_LOG();
        OnAuthResult(P);
    }
}

void xProxyAccessService::ProcessReadyAcquireDeviceFuture() {
    auto & FutureList = AcquireDeviceFutureManager.GetReadyFutureList();
    while (auto P = static_cast<xPA_AcquireDeviceFuture *>(FutureList.PopHead())) {
        DEBUG_LOG();
        (void)P;
        AcquireDeviceFutureManager.ReleaseFuture(P);
    }
}

void xProxyAccessService::ProcessReadyAcquireDeviceConnectionFuture() {
    auto & FutureList = AcquireDeviceConnectionFutureManager.GetReadyFutureList();
    while (auto P = static_cast<xPA_AcquireDeviceConnectionFuture *>(FutureList.PopHead())) {
        DEBUG_LOG();
        (void)P;
        AcquireDeviceConnectionFutureManager.ReleaseFuture(P);
    }
}

void xProxyAccessService::ProcessReadyAcquireDeviceUdpChannelFuture() {
    auto & FutureList = AcquireDeviceUdpChannelFutureManager.GetReadyFutureList();
    while (auto P = static_cast<xPA_AcquireDeviceUdpChannelFuture *>(FutureList.PopHead())) {
        DEBUG_LOG();
        (void)P;
        AcquireDeviceUdpChannelFutureManager.ReleaseFuture(P);
    }
}

std::string xProxyAccessService::OutputAudit() {
    auto SS = std::ostringstream();
    SS << "xProxyAccessService:" << endl;
    SS << "\tInvalidS5AuthTypeCount=" << Audit.InvalidS5AuthTypeCount << endl;
    SS << "\tInvalidS5AuthInfo=" << Audit.InvalidS5AuthInfo << endl;
    SS << "\tInvalidS5AuthResult=" << Audit.InvalidS5AuthResult << endl;
    SS << "\tInvalidS5Target=" << Audit.InvalidS5Target << endl;
    SS << "\tLocalBindUdpChannelCount=" << Audit.LocalBindUdpChannelCount << endl;
    SS << "\tRequestAuthenticationOOM=" << Audit.RequestAuthenticationOOM << endl;
    SS << "\tInitConnectionTimeoutCount=" << Audit.InitConnectionTimeoutCount << endl;
    Reset(Audit);
    SS << "\tAuthFutureCount=" << AuthFutureManager.GetActiveFutureCount() << endl;
    SS << "\tAcquireDeviceFutureCount=" << AcquireDeviceFutureManager.GetActiveFutureCount() << endl;
    SS << "\tAcquireDeviceConnectionFutureCount=" << AcquireDeviceConnectionFutureManager.GetActiveFutureCount() << endl;
    SS << "\tAcquireDeviceUdpChannelFutureCount=" << AcquireDeviceUdpChannelFutureManager.GetActiveFutureCount() << endl;
    return SS.str();
}

void xProxyAccessService::EnableUdp4(const xNetAddress & BindAddress, const xNetAddress & ExportAddress) {
    X_RUNTIME_ASSERT(BindAddress.Is4() && !BindAddress.Port && ExportAddress.Is4() && !ExportAddress.Port);
    BindUdpAddress4   = BindAddress;
    ExportUdpAddress4 = ExportAddress;
    Logger->I("EnableUdp4: %s -> %s", BindAddress.IpToString().c_str(), ExportAddress.IpToString().c_str());
}

void xProxyAccessService::EnableUdp6(const xNetAddress & BindAddress, const xNetAddress & ExportAddress) {
    X_RUNTIME_ASSERT(BindAddress.Is6() && !BindAddress.Port && ExportAddress.Is6() && !ExportAddress.Port);
    BindUdpAddress6   = BindAddress;
    ExportUdpAddress6 = ExportAddress;
    Logger->I("EnableUdp6: %s -> %s", BindAddress.IpToString().c_str(), ExportAddress.IpToString().c_str());
}

void xProxyAccessService::KeepAlive(xPA_ClientConnection * Connection) {
    if (Connection->DeleteMark) {
        return;
    }
    Connection->TimestampMS = LocalTicker();
    ClientConnectionTimeoutList.GrabTail(*Connection);
}

void xProxyAccessService::DeferKill(xPA_ClientConnection * Connection) {
    if (Steal(Connection->DeleteMark, true)) {
        return;
    }
    ClientConnectionKillList.GrabTail(*Connection);
}

void xProxyAccessService::CheckAndReleaseAuthFuture(xPA_ClientConnection * Connection) {
    Connection->AuthFuture ? ReleaseAuthFuture(Connection) : Pass();
}

void xProxyAccessService::ReleaseAuthFuture(xPA_ClientConnection * Connection) {
    auto Future = Steal(Connection->AuthFuture);
    AuthFutureManager.ReleaseFuture(Future);
}

void xProxyAccessService::DestroyConnection(xPA_ClientConnection * Connection) {
    DEBUG_LOG("ConnectionId=%" PRIx64 "", Connection->ConnectionId);
    assert(Connection == ClientConnectionPool.CheckAndGet(Connection->ConnectionId));
    if (auto UdpChannel = Steal(Connection->BindUdpChannel)) {
        DEBUG_LOG("close bind udp channel");
        assert(UdpChannel == ClientUdpChannelPool.CheckAndGet(UdpChannel->UdpChannelId));
        UdpChannel->Clean();
        ClientUdpChannelPool.Release(UdpChannel->UdpChannelId);
    }
    CheckAndReleaseAuthFuture(Connection);
    auto ConnectionId = Connection->ConnectionId;
    Connection->Clean();
    ClientConnectionPool.Release(ConnectionId);
}

void xProxyAccessService::ClearTimeoutFuture() {
    auto KillTimepoint = LocalTicker() - PA_FUTURE_TIMEOUT_MS;
    auto Cond          = [this, KillTimepoint](const xFutureNode & F) {
        return F.StartTimestampMS <= KillTimepoint;
    };
    while (auto P = static_cast<xPA_AuthFuture *>(AuthFutureTimeoutList.PopHead(Cond))) {
        OnAuthResult(P);
    }
    while (auto P = static_cast<xPA_AcquireDeviceFuture *>(AcquireDeviceFutureTimeoutList.PopHead(Cond))) {
        Pass(P);
        // AcquireDeviceFutureManager.ReleaseFuture(P);
    }
    while (auto P = static_cast<xPA_AcquireDeviceConnectionFuture *>(AcquireDeviceConnectionFutureTimeoutList.PopHead(Cond))) {
        Pass(P);
        // AcquireDeviceConnectionFutureManager.ReleaseFuture(P);
    }
    while (auto P = static_cast<xPA_AcquireDeviceUdpChannelFuture *>(AcquireDeviceUdpChannelFutureTimeoutList.PopHead(Cond))) {
        Pass(P);
        // AcquireDeviceUdpChannelFutureManager.ReleaseFuture(P);
    }
}

void xProxyAccessService::DeferKillInitTimeoutConnection() {
    auto KillTimepoint = LocalTicker() - PA_CLIENT_AUTH_TIMEOUT_MS;
    auto Cond          = [this, KillTimepoint](const xPA_ClientConnectionTimeoutNode & N) {
        return N.TimestampMS <= KillTimepoint;
    };
    while (auto P = ClientConnectionInitTimeoutList.PopHead(Cond)) {
        ClientConnectionKillList.AddTail(*P);
        ++Audit.InitConnectionTimeoutCount;
    }
}

void xProxyAccessService::ExcuteKillConnection() {
    while (auto Connection = static_cast<xPA_ClientConnection *>(ClientConnectionKillList.PopHead())) {
        DestroyConnection(Connection);
    }
}

// tcp server listener:
void xProxyAccessService::OnNewConnection(xTcpServer * TcpServerPtr, xSocket && NativeHandle) {
    auto ConnectionId = ClientConnectionPool.Acquire();
    if (!ConnectionId) {
        XelCloseSocket(NativeHandle);
        return;
    }
    auto & ClientConnection = ClientConnectionPool[ConnectionId];
    if (!ClientConnection.Init(ServiceIoContext, std::move(NativeHandle), this)) {
        ClientConnectionPool.Release(ConnectionId);
        return;
    }
    ClientConnection.ConnectionId  = ConnectionId;
    ClientConnection.DataProcessor = &xProxyAccessService::OnGuessProxyType;
    ClientConnection.TimestampMS   = LocalTicker();
    ClientConnectionInitTimeoutList.AddTail(ClientConnection);
}

void xProxyAccessService::OnConnected(xTcpConnection * TcpConnectionPtr) {
    Unreachable();
}

void xProxyAccessService::OnPeerClose(xTcpConnection * TcpConnectionPtr) {
    auto ClientConnection = static_cast<xPA_ClientConnection *>(TcpConnectionPtr);
    DeferKill(ClientConnection);
}

void xProxyAccessService::OnFlush(xTcpConnection * TcpConnectionPtr) {
    Pass();  // speed control
}

size_t xProxyAccessService::OnData(xTcpConnection * TcpConnectionPtr, ubyte * DataPtr, size_t DataSize) {
    auto ClientConnection = static_cast<xPA_ClientConnection *>(TcpConnectionPtr);
    return (this->*ClientConnection->DataProcessor)(ClientConnection, DataPtr, DataSize);
}

void xProxyAccessService::OnData(xUdpChannel * UdpChannelPtr, ubyte * DataPtr, size_t DataSize, const xNetAddress & RemoteAddress) {
    auto ClientUdpChannel = static_cast<xPA_ClientUdpChannel *>(UdpChannelPtr);
    assert(ClientUdpChannel->ParentConnection);
    KeepAlive(ClientUdpChannel->ParentConnection);

    // todo: relay message;
    Pass();
}

size_t xProxyAccessService::KillConnectionOnData(xPA_ClientConnection * Connection, ubyte * DataPtr, size_t DataSize) {
    return InvalidDataSize;
}

size_t xProxyAccessService::OnGuessProxyType(xPA_ClientConnection * Connection, ubyte * DataPtr, size_t DataSize) {
    if (DataSize < 3) {
        DEBUG_LOG("invalid challenge size");
        return InvalidDataSize;
    }
    DEBUG_LOG("OnGuessProxyType: ConnectionId=%" PRIx64 "\n%s", Connection->ConnectionId, xel::HexShow({ (const char *)DataPtr, DataSize }).c_str());
    if ('\x05' == (char)DataPtr[0]) {
        return OnS5Challenge(Connection, DataPtr, DataSize);
    }
    if ('\x16' == (char)DataPtr[0]) {  // may be TLS test
        ubyte Buffer[] = { '\x15', '\x03', '\x01', '\x00', '\x02', '\x02', '\x28' };
        Connection->PostData(Buffer, Length(Buffer));
        return 0;
    }
    return OnHttpChallenge(Connection, DataPtr, DataSize);
}

size_t xProxyAccessService::OnS5Challenge(xPA_ClientConnection * Connection, ubyte * DataPtr, size_t DataSize) {
    xStreamReader R(DataPtr);
    R.Skip(1);  // skip type check bytes

    size_t NM = R.R1();  // number of methods
    if (!NM) {
        return InvalidDataSize;
    }
    size_t HeaderSize = 2 + NM;
    if (DataSize < HeaderSize) {
        return InvalidDataSize;
    }
    bool UserPassSupport = false;
    for (size_t i = 0; i < NM; ++i) {
        uint8_t Method = R.R1();
        if (Method == 0x02) {
            UserPassSupport = true;
            break;
        }
    }
    Connection->Type = xPA_ClientConnection::S5_CHALLENGE;
    if (!UserPassSupport) {
        auto RemoteAddress        = Connection->GetRemoteAddress();
        auto Auth                 = '\x00' + RemoteAddress.IpToString();
        Connection->NoAuth        = true;
        Connection->DataProcessor = &xProxyAccessService::KillConnectionOnData;
        auto Future               = RequestAuthentication(Connection, Auth);
        if (!(Connection->AuthFuture = Future)) {
            DEBUG_LOG("AuthFutureManager OOM");
            ++Audit.RequestAuthenticationOOM;
            Connection->PostData("\x05\xFF", 2);  // auth failure
            return HeaderSize;                    // wait for client side close
        }
        return HeaderSize;
    }

    DEBUG_LOG("Accept account/pass authentication");
    ubyte Socks5Auth[2] = { 0x05, 0x02 };
    Connection->PostData(Socks5Auth, sizeof(Socks5Auth));
    Connection->DataProcessor = &xProxyAccessService::OnS5AuthInfo;
    return HeaderSize;
}

size_t xProxyAccessService::OnS5AuthInfo(xPA_ClientConnection * Connection, ubyte * DataPtr, size_t DataSize) {
    DEBUG_LOG("\n%s", HexShow(DataPtr, DataSize).c_str());
    if (DataSize < 3) {
        return 0;
    }
    xStreamReader R(DataPtr);
    auto          Ver = R.R1();
    if (Ver != 0x01) {  // only version for user pass
        ++Audit.InvalidS5AuthTypeCount;
        return InvalidDataSize;
    }
    size_t NameLen = R.R1();
    if (DataSize < 3 + NameLen) {
        return 0;
    }
    char * KeyStart = (char *)DataPtr + R.Offset();
    R.Skip(NameLen);

    size_t PassLen = R.R1();
    if (DataSize < 3 + NameLen + PassLen) {
        return 0;
    }
    ((char *)DataPtr)[R.Offset() - 1] = '\0';  // make quick user/pass key
    R.Skip(PassLen);

    if (!NameLen || !PassLen) {
        ++Audit.InvalidS5AuthInfo;
        return InvalidDataSize;
    }
    Connection->DataProcessor = &xProxyAccessService::KillConnectionOnData;

    size_t KeyLength = NameLen + 1 + PassLen;
    auto   KeyView   = std::string_view{ KeyStart, KeyLength };
    DEBUG_LOG("Auth with Account: %s", StrToHex(KeyView).c_str());
    auto Future = RequestAuthentication(Connection, KeyView);
    if (!(Connection->AuthFuture = Future)) {
        DEBUG_LOG("AuthFutureManager OOM");
        ++Audit.RequestAuthenticationOOM;
        Connection->PostData("\x01\x01", 2);  // auth failure
        return R.Offset();
    }
    return R.Offset();
}

size_t xProxyAccessService::OnS5Target(xPA_ClientConnection * Connection, ubyte * DataPtr, size_t DataSize) {
    if (DataSize < 10) {
        return 0;
    }
    if (DataSize >= 6 + 256) {
        ++Audit.InvalidS5Target;
        return InvalidDataSize;
    }
    DEBUG_LOG("\n%s", HexShow(DataPtr, DataSize).c_str());

    xStreamReader R(DataPtr);
    uint8_t       Version   = R.R();
    uint8_t       Operation = R.R();
    uint8_t       Reserved  = R.R();
    uint8_t       AddrType  = R.R();
    if (Version != 0x05 || Reserved != 0x00) {
        DEBUG_LOG("invalid protocol");
        ++Audit.InvalidS5Target;
        return InvalidDataSize;
    }
    // extract target info:
    auto   TargetAddress = xNetAddress();
    char   DomainName[256];
    size_t DomainNameLength = 0;
    if (AddrType == 0x01) {  // ipv4
        TargetAddress.Type = xNetAddress::IPV4;
        R.R(TargetAddress.SA4, 4);
        TargetAddress.Port = R.R2();

        DEBUG_LOG("target address: %s", TargetAddress.ToString().c_str());
    } else if (AddrType == 0x04) {  // ipv6
        if (DataSize < 4 + 16 + 2) {
            return 0;
        }
        TargetAddress.Type = xNetAddress::IPV6;
        R.R(TargetAddress.SA6, 16);
        TargetAddress.Port = R.R2();

        DEBUG_LOG("target address: %s", TargetAddress.ToString().c_str());
    } else if (AddrType == 0x03) {
        DomainNameLength = R.R();
        if (DataSize < 4 + 1 + DomainNameLength + 2) {
            return 0;
        }
        R.R(DomainName, DomainNameLength);
        DomainName[DomainNameLength] = '\0';
        TargetAddress.Port           = R.R2();
        DEBUG_LOG("target domain: %s, port=%u", DomainName, (unsigned)TargetAddress.Port);
    }
    if (Operation == 0x01) {  // build tcp connection
        DEBUG_LOG("Operation: Connection");
        auto Future = AcquireDeviceConnectionFutureManager.AcquireFuture();
        AcquireDeviceConnectionFutureManager.ReleaseFuture(Steal(Future));
        if (!Future) {
            if (AddrType = 0x01) {  // ipv4
                Connection->PostData(S5_CONNECTION_IPV4_FAILED, sizeof(S5_CONNECTION_IPV4_FAILED));
                return 0;
            } else if (AddrType == 0x04) {  // ipv6
                Connection->PostData(S5_CONNECTION_IPV6_FAILED, sizeof(S5_CONNECTION_IPV6_FAILED));
                return 0;
            }
        }

    } else if (Operation == 0x03) {  // udp
        DEBUG_LOG("Operation: UdpChannel");
    } else {
        size_t        AddressLength = R.Offset() - 3;
        ubyte         Buffer[512];
        xStreamWriter W(Buffer);
        W.W(0x05);
        W.W(0x01);
        W.W(0x00);
        W.W((ubyte *)DataPtr + 3, AddressLength);
        Connection->PostData(W.Origin(), W.Offset());
        return 0;
    }
    return 0;
}

size_t xProxyAccessService::OnHttpChallenge(xPA_ClientConnection * Connection, ubyte * DataPtr, size_t DataSize) {
    return InvalidDataSize;
}

xPA_AuthFuture * xProxyAccessService::RequestAuthentication(xPA_ClientConnection * Connection, std::string_view AuthView) {
    auto Future = AuthFutureManager.AcquireFuture();
    if (!Future) {
        return nullptr;
    }
    assert(AuthService);
    Future->ClientConnection = Connection;
    AuthService->Validate(AuthView, *Future);
    return Future;
}

xDeviceRequest xProxyAccessService::ConvertAuthResultToDeviceRequirement(const xAuthResult & AuthResult) {
    return {};
}

xPA_AcquireDeviceFuture * xProxyAccessService::RequestDevice(xPA_ClientConnection * Connection, const xDeviceRequest & Request) {
    auto Future = AcquireDeviceFutureManager.AcquireFuture();
    if (!Future) {
        return nullptr;
    }
    assert(DeviceLocatorService);
    Future->ClientConnection = Connection;
    DeviceLocatorService->AcquireDevice(Request, *Future);
    return Future;
}

void xProxyAccessService::OnAuthResult(xPA_AuthFuture * Future) {
    auto Connection = Future->ClientConnection;
    assert(Connection->AuthFuture == Future);
    assert(Future->IsReady);
    X_SCOPE_GUARD([=, this] {
        ReleaseAuthFuture(Connection);
    });
    if (Connection->Type == xPA_ClientConnection::eType::S5_CHALLENGE) {
        return OnS5AuthResult(Connection, Future);
    }
}

void xProxyAccessService::OnS5AuthResult(xPA_ClientConnection * Connection, xPA_AuthFuture * Future) {
    DEBUG_LOG();

    auto Result = Future->Result ? &*Future->Result : nullptr;
    if (Connection->NoAuth) {
        if (!Result || !Result->ProxyAccessAddress) {
            Connection->PostData("\x05\xFF", 2);  // no-auth failed
        } else {
            Connection->PostData("\x05\x00", 2);
            Connection->DataProcessor = &xProxyAccessService::OnS5Target;
        }
        DEBUG_LOG("%s", Result->ToString().c_str());
    } else {
        if (!Result || !Result->ProxyAccessAddress) {
            Connection->PostData("\x01\x01", 2);  // auth failure
        } else {
            Connection->PostData("\x01\x00", 2);
            Connection->DataProcessor = &xProxyAccessService::OnS5Target;
        }
        DEBUG_LOG("%s", Result->ToString().c_str());
    }
}
