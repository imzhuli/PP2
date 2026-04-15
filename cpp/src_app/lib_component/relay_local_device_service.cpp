#include "./relay_local_device_service.hpp"

#include <pp_common/service_runtime.hpp>

static constexpr const size_t DEVICE_ID_HIGH32_MAGIC       = 0xCDEF7788;
static constexpr const size_t MAX_MANAGED_CONNECTION_SIZE  = 15'0000;
static constexpr const size_t MAX_MANAGED_UDP_CHANNEL_SIZE = 5'0000;

static uint64_t MakeLocalDeviceId(size_t Index) {
    RuntimeAssert(Index < std::numeric_limits<uint32_t>::max());
    return Make64(DEVICE_ID_HIGH32_MAGIC, Index);
}

static size_t EstimateMaxConnectionPoolSize(size_t LocalAddressSize) {
    return std::min(4'0000 * LocalAddressSize, MAX_MANAGED_CONNECTION_SIZE);
}
static size_t EstimateMaxUdpChannelPoolSize(size_t LocalAddressSize) {
    return std::min(3'0000 * LocalAddressSize, MAX_MANAGED_UDP_CHANNEL_SIZE);
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

void xRelayLocalBindingService::DeferDestroyConnection(uint64_t ConnectionId) {
    auto Connection = LocalConnectionPool.CheckAndGet(ConnectionId);
    if (!Connection) {
        return;
    }
    DeferDestroyConnection(Connection);
}

void xRelayLocalBindingService::KeepAlive(xRelayLocalDeviceConnection * Connection) {
    if (Connection->DeleteMark) {
        return;
    }
    Connection->TimestampMS = LocalTicker();
    ConnectionIdleTimeoutList.GrabTail(*Connection);
}

void xRelayLocalBindingService::DeferDestroyConnection(xRelayLocalDeviceConnection * Connection) {
    assert(Connection == LocalConnectionPool.CheckAndGet(Connection->ConnectionId));
    if (!Steal(Connection->DeleteMark, true)) {
        ConnectionIdleKillList.GrabTail(*Connection);
    }
}

void xRelayLocalBindingService::DestroyAllConnections() {
    ConnectionIdleKillList.GrabListTail(ConnectionEstablishTimeoutList);
    ConnectionIdleKillList.GrabListTail(ConnectionIdleTimeoutList);
    while (auto P = static_cast<xRelayLocalDeviceConnection *>(ConnectionIdleKillList.PopHead())) {
        P->Clean();
        LocalConnectionPool.Release(P->ConnectionId);
    }
}

void xRelayLocalBindingService::DestroyAllUdpChannels() {
    UdpChannelKillList.GrabListTail(UdpChannelTimeoutList);
    while (auto P = static_cast<xRelayLocalDeviceUdpChannel *>(UdpChannelKillList.PopHead())) {
        P->Clean();
        LocalUdpChannelPool.Release(P->ChannelId);
    }
}

void xRelayLocalBindingService::OnConnected(xTcpConnection * TcpConnectionPtr) {
    auto Connection = static_cast<xRelayLocalDeviceConnection *>(TcpConnectionPtr);
    assert(Connection->CreateConnectionFutureHandle);
}

void xRelayLocalBindingService::OnPeerClose(xTcpConnection * TcpConnectionPtr) {
    auto Connection = static_cast<xRelayLocalDeviceConnection *>(TcpConnectionPtr);
    if (Connection->IsConnecting()) {
        assert(Connection->CreateConnectionFutureHandle);
        auto Future = Connection->CreateConnectionFutureHandle.Get<xRelayCreateConnectionFuture>();
        assert(Future);
        Future->SetReady();
    }
    Pass(Connection);
    Pure();
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