#include "./relay_local_device_service.hpp"

#include <pp_common/service_runtime.hpp>

static constexpr const size_t DEVICE_ID_HIGH32_MAGIC      = 0xCDEF7788;
static constexpr const size_t MAX_MANAGED_CONNECTION_SIZE = 15'0000;
static constexpr const size_t MAX_MANAGED_UDPCHANNEL_SIZE = 10'0000;
static constexpr const size_t IDLE_CONNECTION_TIMEOUT_MS  = 120'000;
static constexpr const size_t IDLE_UDPCHANNEL_TIMEOUT_MS  = 120'000;

static uint64_t MakeLocalDeviceId(size_t Index) {
    X_RUNTIME_ASSERT(Index < std::numeric_limits<uint32_t>::max());
    return Make64(DEVICE_ID_HIGH32_MAGIC, Index);
}

static size_t ExtractIndexFromDeviceId(uint64_t DeviceId) {
    assert((DeviceId >> 32) == DEVICE_ID_HIGH32_MAGIC);
    return DeviceId & 0xFFFFFFFF;
}

static size_t EstimateMaxConnectionPoolSize(size_t LocalAddressSize) {
    return std::min(4'0000 * LocalAddressSize, MAX_MANAGED_CONNECTION_SIZE);
}
static size_t EstimateMaxUdpChannelPoolSize(size_t LocalAddressSize) {
    return std::min(6'0000 * LocalAddressSize, MAX_MANAGED_UDPCHANNEL_SIZE);
}

std::string xRelayLocalBindingOption::ToString() const {
    auto OS = std::ostringstream();
    OS << "[ ";
    OS << "B4:" << BindAddress4.ToString() << " ";
    OS << "E4:" << ExportAddress4.ToString() << " ";
    OS << "B6:" << BindAddress6.ToString() << " ";
    OS << "E6:" << ExportAddress6.ToString() << " ";
    OS << (EnableTcp ? "tcp" : "no-tcp") << " ";
    OS << (EnableUdp ? "udp" : "no-udp") << " ";
    OS << "]";
    return OS.str();
}

bool xRelayLocalBindingService::Init(uint64_t ServerId, const std::string & AddressPairFile) {
    // parse file:
    auto BindAddressPairList = std::vector<xRelayLocalBindingOption>();
    auto Lines               = xel::FileToLines(AddressPairFile);
    for (auto L : Lines) {
        L = xel::Trim(L);
        if (L.empty()) {
            continue;
        }
        DEBUG_LOG("AddressPairLine:%s", L.c_str());
        auto Segs = xel::Split(L, "|");
        // [0]: addr4 mapping
        // [1]: addr6 mapping
        // [2]: functions
        if (Segs.size() != 3) {
            Logger->F("invalid config line");
            return false;
        }
        auto A4M    = Split(Segs[0], "->");
        auto A6M    = Split(Segs[1], "->");
        auto FN     = Split(Segs[2], ",");
        auto Option = xRelayLocalBindingOption();
        do {  // 4
            if (A4M.empty()) {
                break;
            }
            X_RUNTIME_ASSERT(A4M.size() == 2);
            auto B = xNetAddress::Parse(Trim(A4M[0]));
            auto E = xNetAddress::Parse(Trim(A4M[1]));
            X_RUNTIME_ASSERT(B.Is4() && E.Is4());
            Option.BindAddress4   = B;
            Option.ExportAddress4 = E;
        } while (false);
        do {  // 6
            if (A6M.empty()) {
                break;
            }
            X_RUNTIME_ASSERT(A6M.size() == 2);
            auto B = xNetAddress::Parse(Trim(A6M[0]));
            auto E = xNetAddress::Parse(Trim(A6M[1]));
            X_RUNTIME_ASSERT(B.Is6() && E.Is6());
            Option.BindAddress6   = B;
            Option.ExportAddress6 = E;
        } while (false);
        X_RUNTIME_ASSERT(Option.BindAddress4 || Option.BindAddress6);
        do {
            for (auto & F : FN) {
                F = Trim(F);
                if (!strcmp(F.c_str(), "tcp")) {  // enable tcp
                    X_RUNTIME_ASSERT(!Steal(Option.EnableTcp, true));
                }
                if (!strcmp(F.c_str(), "udp")) {  // enable tcp
                    X_RUNTIME_ASSERT(!Steal(Option.EnableUdp, true));
                }
                X_RUNTIME_ASSERT(Option.EnableTcp || Option.EnableUdp);
            }
        } while (false);
        BindAddressPairList.push_back(Option);
    }
    return Init(ServerId, BindAddressPairList);
}

bool xRelayLocalBindingService::Init(uint64_t ServerId, const std::vector<xRelayLocalBindingOption> & BindAddressPairList) {
    X_RUNTIME_ASSERT(ServerId);
    X_RUNTIME_ASSERT(ServiceRunState);
    if (!CreateLocalDeviceList(BindAddressPairList)) {
        return false;
    }
    if (!LocalConnectionPool.Init(EstimateMaxConnectionPoolSize(BindAddressPairList.size()))) {
        DestroyLocalDeviceList();
        return false;
    }
    if (!LocalUdpChannelPool.Init(EstimateMaxUdpChannelPoolSize(BindAddressPairList.size()))) {
        LocalConnectionPool.Clean();
        DestroyLocalDeviceList();
        return false;
    }
    LocalRelayServerId = ServerId;
    return true;
}

void xRelayLocalBindingService::Clean() {
    DestroyAllConnections();
    LocalUdpChannelPool.Clean();
    DestroyAllUdpChannels();
    LocalConnectionPool.Clean();
    DestroyLocalDeviceList();

    Reset(Audit);
}

void xRelayLocalBindingService::Tick(uint64_t NowMS) {
    LocalTicker.Update(NowMS);
    DeferDestroyIdleConnections();
    DeferDestroyIdleUdpChannels();
}

bool xRelayLocalBindingService::CreateLocalDeviceList(const std::vector<xRelayLocalBindingOption> & OptionList) {
    X_RUNTIME_ASSERT(LocalDeviceList.empty());
    size_t IndexCount = 0;
    for (auto & O : OptionList) {
        auto DInfo = xRelayLocalDevice{
            .DeviceId       = MakeLocalDeviceId(IndexCount++),
            .BindAddress4   = O.BindAddress4,
            .ExportAddress4 = O.ExportAddress4,
            .BindAddress6   = O.BindAddress6,
            .ExportAddress6 = O.ExportAddress6,
            .EnableTcp      = O.EnableTcp,
            .EnableUdp      = O.EnableUdp,
        };
        if (DInfo.ExportAddress4) {
            auto & E = LocalDeviceExportAddressMap[DInfo.ExportAddress4];
            X_RUNTIME_ASSERT(!Steal(E, DInfo.DeviceId));
        }
        if (DInfo.ExportAddress6) {
            auto & E = LocalDeviceExportAddressMap[DInfo.ExportAddress6];
            X_RUNTIME_ASSERT(!Steal(E, DInfo.DeviceId));
        }
        LocalDeviceList.push_back(DInfo);
    }
    return true;
}

void xRelayLocalBindingService::DestroyLocalDeviceList() {
    Reset(LocalDeviceList);
    Reset(LocalDeviceExportAddressMap);
}

const xRelayLocalDevice * xRelayLocalBindingService::GetDevice(uint64_t DeviceId) const {
    auto H32 = High32(DeviceId);
    auto L32 = Low32(DeviceId);
    if (H32 != DEVICE_ID_HIGH32_MAGIC) {
        return nullptr;
    }
    if (L32 >= LocalDeviceList.size()) {
        return nullptr;
    }
    return &LocalDeviceList[L32];
}

bool xRelayLocalBindingService::AcquireDevice(const xDeviceRequest & Request, xAcquireDeviceFuture & Future) {
    return false;
}

void xRelayLocalBindingService::CreateConnection(uint64_t RelayServerId, uint64_t DeviceId, uint64_t PASideConnectionId, const xNetAddress & TargetAddress, xRelayCreateConnectionFuture & Future) {
    assert(PASideConnectionId);
    auto Device = GetDevice(DeviceId);
    if (!Device) {
        Future.SetReady();
        return;
    }
    auto ConnectionId = LocalConnectionPool.Acquire();
    if (!ConnectionId) {
        Future.SetReady();
        return;
    }
    auto & Connection = LocalConnectionPool[ConnectionId];
    if (!Connection.Init(ServiceIoContext, TargetAddress, TargetAddress.Is4() ? Device->BindAddress4 : Device->BindAddress6, this)) {
        LocalConnectionPool.Release(ConnectionId);
        Future.SetReady();
        return;
    }
    Connection.DeviceId                     = DeviceId;
    Connection.ConnectionId                 = ConnectionId;
    Connection.PASideConnectionId           = PASideConnectionId;
    Connection.CreateConnectionFutureHandle = xFutureHandle(Future);

    ++Audit.ConnectionCount;
    return;
}

void xRelayLocalBindingService::CreateUdpChannel(uint64_t RelayServerId, uint64_t DeviceId, uint64_t PASideUdpChannelId, xNetAddress::eType Type, xRelayCreateUdpChannelFuture & Future) {
    assert(Type == xNetAddress::IPV4 || Type == xNetAddress::IPV6);
    Future.SetReady();  // always has immediate result
    auto Device = GetDevice(DeviceId);
    if (!Device) {
        return;
    }
    auto UdpChannelId = LocalUdpChannelPool.Acquire();
    if (!UdpChannelId) {
        return;
    }
    auto & UdpChannel = LocalUdpChannelPool[UdpChannelId];
    if (!UdpChannel.Init(ServiceIoContext, (Type == xNetAddress::IPV4) ? Device->BindAddress4 : Device->BindAddress6, this)) {
        LocalUdpChannelPool.Release(UdpChannelId);
        return;
    }
    UdpChannel.UdpChannelId       = UdpChannelId;
    UdpChannel.PASideUdpChannelId = PASideUdpChannelId;

    ++Audit.UdpChannelCount;
    return;
}

void xRelayLocalBindingService::DestroyConnection(uint64_t RelayServerId, uint64_t ConnectionId) {
    auto Connection = LocalConnectionPool.CheckAndGet(ConnectionId);
    if (!Connection) {
        return;
    }
    DeferDestroyConnection(Connection);
}

void xRelayLocalBindingService::DestroyUdpChannel(uint64_t RelayServerId, uint64_t UdpChannelId) {
    auto UdpChannel = LocalUdpChannelPool.CheckAndGet(UdpChannelId);
    if (!UdpChannel) {
        return;
    }
    DeferDestroyUdpChannel(UdpChannel);
}

////////////////////////////////

void xRelayLocalBindingService::KeepAlive(xRelayLocalDeviceConnection * Connection) {
    if (Connection->DeleteMark) {
        return;
    }
    Connection->TimestampMS = LocalTicker();
    ConnectionIdleTimeoutList.GrabTail(*Connection);
}

void xRelayLocalBindingService::KeepAlive(xRelayLocalDeviceUdpChannel * UdpChannel) {
    if (UdpChannel->DeleteMark) {
        return;
    }
    UdpChannel->TimestampMS = LocalTicker();
    UdpChannelIdleTimeoutList.GrabTail(*UdpChannel);
}

void xRelayLocalBindingService::DeferDestroyConnection(xRelayLocalDeviceConnection * Connection) {
    assert(Connection == LocalConnectionPool.CheckAndGet(Connection->ConnectionId));
    if (!Steal(Connection->DeleteMark, true)) {
        if (Connection->CreateConnectionFutureHandle) {
            if (auto Future = Steal(Connection->CreateConnectionFutureHandle).Get<xRelayCreateConnectionFuture>()) {
                Future->SetReady();
            }
        }
        ConnectionKillList.GrabTail(*Connection);
    }
}

void xRelayLocalBindingService::DeferDestroyUdpChannel(xRelayLocalDeviceUdpChannel * UdpChannel) {
    assert(UdpChannel == LocalUdpChannelPool.CheckAndGet(UdpChannel->UdpChannelId));
    if (!Steal(UdpChannel->DeleteMark, true)) {
        UdpChannelKillList.GrabTail(*UdpChannel);
    }
}

void xRelayLocalBindingService::DeferDestroyIdleConnections() {
    auto NowMS = LocalTicker();
    auto Cond  = [KillTimepoint = NowMS - IDLE_CONNECTION_TIMEOUT_MS](const xRelayLocalDeviceConnectionTimeoutNode & N) {
        return N.TimestampMS <= KillTimepoint;
    };
    while (auto P = static_cast<xRelayLocalDeviceConnection *>(ConnectionIdleTimeoutList.PopHead(Cond))) {
        DeferDestroyConnection(P);
    }
}

void xRelayLocalBindingService::DeferDestroyIdleUdpChannels() {
    auto NowMS = LocalTicker();
    auto Cond  = [KillTimepoint = NowMS - IDLE_CONNECTION_TIMEOUT_MS](const xRelayLocalDeviceUdpChannelTimeoutNode & N) {
        return N.TimestampMS <= KillTimepoint;
    };
    while (auto P = static_cast<xRelayLocalDeviceUdpChannel *>(UdpChannelIdleTimeoutList.PopHead(Cond))) {
        DeferDestroyUdpChannel(P);
    }
}

void xRelayLocalBindingService::DestroyAllConnections() {
    ConnectionKillList.GrabListTail(ConnectionEstablishTimeoutList);
    ConnectionKillList.GrabListTail(ConnectionIdleTimeoutList);
    while (auto P = static_cast<xRelayLocalDeviceConnection *>(ConnectionKillList.PopHead())) {
        P->Clean();
        LocalConnectionPool.Release(P->ConnectionId);
        --Audit.ConnectionCount;
    }
}

void xRelayLocalBindingService::DestroyAllUdpChannels() {
    UdpChannelKillList.GrabListTail(UdpChannelIdleTimeoutList);
    while (auto P = static_cast<xRelayLocalDeviceUdpChannel *>(UdpChannelKillList.PopHead())) {
        P->Clean();
        LocalUdpChannelPool.Release(P->UdpChannelId);
        --Audit.UdpChannelCount;
    }
}

////////////////////////////////

const xRelayLocalDevice * xRelayLocalBindingService::FindDeviceByExportAddress(const xNetAddress & ExportAddress) {
    auto Iter = LocalDeviceExportAddressMap.find(ExportAddress);
    if (Iter == LocalDeviceExportAddressMap.end()) {
        return nullptr;
    }
    auto DeviceId    = Iter->second;
    auto DeviceIndex = ExtractIndexFromDeviceId(DeviceId);
    return &LocalDeviceList[DeviceIndex];
}

////////////////////////////////

void xRelayLocalBindingService::OnConnected(xTcpConnection * TcpConnectionPtr) {
    auto Connection = static_cast<xRelayLocalDeviceConnection *>(TcpConnectionPtr);
    if (auto Future = !Connection->CreateConnectionFutureHandle ? nullptr : Steal(Connection->CreateConnectionFutureHandle).Get<xRelayCreateConnectionFuture>()) {
        Future->RelaySideConnectionId = Connection->ConnectionId;
        Future->SetReady();
        KeepAlive(Connection);
    }
}

void xRelayLocalBindingService::OnPeerClose(xTcpConnection * TcpConnectionPtr) {
    auto Connection = static_cast<xRelayLocalDeviceConnection *>(TcpConnectionPtr);
    DeferDestroyConnection(Connection);
}

void xRelayLocalBindingService::PostData(uint64_t RelayServerId, uint64_t ConnectionId, const void * Payload, size_t PayloadSize) {
    auto Connection = LocalConnectionPool.CheckAndGet(ConnectionId);
    if (!Connection || Connection->DeleteMark || !Connection->IsConnected()) {
        return;
    }
    Connection->PostData(Payload, PayloadSize);
}

void xRelayLocalBindingService::PostData(uint64_t RelayServerId, uint64_t UdpChannelId, const xel::xNetAddress & TargetAddress, const void * Payload, size_t PayloadSize) {
    auto UdpChannel = LocalUdpChannelPool.CheckAndGet(UdpChannelId);
    if (!UdpChannel || UdpChannel->DeleteMark) {
        return;
    }
    UdpChannel->PostData(TargetAddress, Payload, PayloadSize);
}