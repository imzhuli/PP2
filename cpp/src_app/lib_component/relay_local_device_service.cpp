#include "./relay_local_device_service.hpp"

#include <pp_common/service_runtime.hpp>

static constexpr const size_t DEVICE_ID_HIGH32_MAGIC      = 0xCDEF7788;
static constexpr const size_t MAX_MANAGED_CONNECTION_SIZE = 15'0000;
static constexpr const size_t MAX_MANAGED_UDPCHANNEL_SIZE = 5'0000;
static constexpr const size_t IDLE_CONNECTION_TIMEOUT_MS  = 120'000;
static constexpr const size_t IDLE_UDPCHANNEL_TIMEOUT_MS  = 120'000;

static uint64_t MakeLocalDeviceId(size_t Index) {
    RuntimeAssert(Index < std::numeric_limits<uint32_t>::max());
    return Make64(DEVICE_ID_HIGH32_MAGIC, Index);
}

static size_t EstimateMaxConnectionPoolSize(size_t LocalAddressSize) {
    return std::min(4'0000 * LocalAddressSize, MAX_MANAGED_CONNECTION_SIZE);
}
static size_t EstimateMaxUdpChannelPoolSize(size_t LocalAddressSize) {
    return std::min(3'0000 * LocalAddressSize, MAX_MANAGED_UDPCHANNEL_SIZE);
}

bool xRelayLocalBindingService::Init(const std::vector<std::pair<xNetAddress, xNetAddress>> & BindAddressPairList) {
    RuntimeAssert(ServiceRunState);
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
    return true;
}

void xRelayLocalBindingService::Clean() {
    DestroyAllConnections();
    LocalUdpChannelPool.Clean();
    DestroyAllUdpChannels();
    LocalConnectionPool.Clean();
    DestroyLocalDeviceList();
}

void xRelayLocalBindingService::Tick(uint64_t NowMS) {
    LocalTicker.Update(NowMS);
    DeferDestroyIdleConnections();
    DeferDestroyIdleUdpChannels();
}

bool xRelayLocalBindingService::CreateLocalDeviceList(const std::vector<std::pair<xNetAddress, xNetAddress>> & BindAddressPairList) {
    RuntimeAssert(LocalDeviceList.empty());
    size_t IndexCount = 0;
    for (auto & P : BindAddressPairList) {
        auto DInfo = xRelayLocalDevice{
            .DeviceId      = MakeLocalDeviceId(IndexCount++),
            .BindAddress   = P.first,
            .ExportAddress = P.second,
        };
        LocalDeviceList.push_back(DInfo);
    }
    return true;
}

void xRelayLocalBindingService::DestroyLocalDeviceList() {
    Reset(LocalDeviceList);
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

void xRelayLocalBindingService::CreateConnection(uint64_t DeviceId, uint64_t PASideConnectionId, const xNetAddress & TargetAddress, xRelayCreateConnectionFuture & Future) {
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
    if (!Connection.Init(ServiceIoContext, TargetAddress, Device->BindAddress, this)) {
        LocalConnectionPool.Release(ConnectionId);
        Future.SetReady();
        return;
    }
    Connection.DeviceId                     = DeviceId;
    Connection.ConnectionId                 = ConnectionId;
    Connection.PASideConnectionId           = PASideConnectionId;
    Connection.CreateConnectionFutureHandle = xFutureHandle(Future);
    return;
}

void xRelayLocalBindingService::CreateUdpChannel(uint64_t DeviceId, uint64_t PASideUdpChannelId, xRelayCreateUdpChannelFuture & Future) {
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
    if (!UdpChannel.Init(ServiceIoContext, Device->BindAddress, this)) {
        LocalUdpChannelPool.Release(UdpChannelId);
        return;
    }
    UdpChannel.UdpChannelId       = UdpChannelId;
    UdpChannel.PASideUdpChannelId = PASideUdpChannelId;
}

void xRelayLocalBindingService::DeferDestroyConnection(uint64_t ConnectionId) {
    auto Connection = LocalConnectionPool.CheckAndGet(ConnectionId);
    if (!Connection) {
        return;
    }
    DeferDestroyConnection(Connection);
}

void xRelayLocalBindingService::DeferDestroyUdpChannel(uint64_t UdpChannelId) {
    auto UdpChannel = LocalUdpChannelPool.Check(UdpChannelId);
    if (!UdpChannelId) {
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
    }
}

void xRelayLocalBindingService::DestroyAllUdpChannels() {
    UdpChannelKillList.GrabListTail(UdpChannelIdleTimeoutList);
    while (auto P = static_cast<xRelayLocalDeviceUdpChannel *>(UdpChannelKillList.PopHead())) {
        P->Clean();
        LocalUdpChannelPool.Release(P->UdpChannelId);
    }
}

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

void xRelayLocalBindingService::PostData(uint64_t ConnectionId, const void * Payload, size_t PayloadSize) {
    auto Connection = LocalConnectionPool.CheckAndGet(ConnectionId);
    if (!Connection || Connection->DeleteMark || !Connection->IsConnected()) {
        return;
    }
    Connection->PostData(Payload, PayloadSize);
}

void xRelayLocalBindingService::PostData(uint64_t UdpChannelId, const xel::xNetAddress & TargetAddress, const void * Payload, size_t PayloadSize) {
    auto UdpChannel = LocalUdpChannelPool.CheckAndGet(UdpChannelId);
    if (!UdpChannel || UdpChannel->DeleteMark) {
        return;
    }
    UdpChannel->PostData(TargetAddress, Payload, PayloadSize);
}