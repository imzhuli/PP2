#include "./relay_local_device_service.hpp"

#include <pp_common/service_runtime.hpp>

static constexpr const size_t DEVICE_ID_HIGH32_MAGIC      = 0xCDEF7788;
static constexpr const size_t MAX_MANAGED_CONNECTION_SIZE = 15'0000;
static constexpr const size_t MAX_MANAGED_UDPCHANNEL_SIZE = 10'0000;
static constexpr const size_t IDLE_CONNECTION_TIMEOUT_MS  = 120'000;
static constexpr const size_t IDLE_UDPCHANNEL_TIMEOUT_MS  = 120'000;
static constexpr const size_t MAX_DNS_FUTURE_COUNT        = 1'0000;

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
    OS << "B:" << BindAddress.ToString() << " ";
    OS << "E:" << ExportAddress.ToString() << " ";
    OS << (EnableUdp ? "udp" : "no-udp") << " ";
    OS << "]";
    return OS.str();
}

std::string xRelayLocalDevice::ToString() const {
    auto OS = std::ostringstream();
    OS << "[ ";
    OS << "DeviceId:" << DeviceId << " ";
    OS << "B:" << BindAddress.ToString() << " ";
    OS << "E:" << ExportAddress.ToString() << " ";
    OS << "EnableTcp:" << YN(EnableTcp) << " ";
    OS << "EnableUdp:" << YN(EnableUdp) << " ";
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
        if (Segs.size() != 2) {
            Logger->F("invalid config line");
            return false;
        }
        auto AM     = Split(Segs[0], "->");
        auto FN     = Split(Segs[1], ",");
        auto Option = xRelayLocalBindingOption();
        X_RUNTIME_ASSERT(AM.size() == 2);
        auto B = xNetAddress::Parse(Trim(AM[0]));
        auto E = xNetAddress::Parse(Trim(AM[1]));
        X_RUNTIME_ASSERT(B && E);
        Option.BindAddress   = B;
        Option.ExportAddress = E;
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
        DEBUG_LOG();
        return false;
    }
    if (!LocalConnectionPool.Init(EstimateMaxConnectionPoolSize(BindAddressPairList.size()))) {
        DEBUG_LOG();
        DestroyLocalDeviceList();
        return false;
    }
    if (!LocalUdpChannelPool.Init(EstimateMaxUdpChannelPoolSize(BindAddressPairList.size()))) {
        DEBUG_LOG();
        LocalConnectionPool.Clean();
        DestroyLocalDeviceList();
        return false;
    }
    if (!DnsFutureManager.Init(MAX_DNS_FUTURE_COUNT)) {
        DEBUG_LOG();
        LocalUdpChannelPool.Clean();
        LocalConnectionPool.Clean();
        DestroyLocalDeviceList();
        return false;
    }

    LocalRelayServerId = ServerId;
    return true;
}

void xRelayLocalBindingService::Clean() {
    DnsFutureManager.Clean();
    Reset(DnsService);
    CleanAllConnections();
    CleanAllUdpChannels();
    LocalUdpChannelPool.Clean();
    LocalConnectionPool.Clean();
    DestroyLocalDeviceList();

    Reset(Audit);
}

void xRelayLocalBindingService::BindDnsService(xDnsAbstractService * DnsService) {
    X_RUNTIME_ASSERT(!Steal(this->DnsService, DnsService));
}

void xRelayLocalBindingService::Tick(uint64_t NowMS) {
    LocalTicker.Update(NowMS);
    ProcessDnsResults();
    DeferDestroyIdleConnections();
    DeferDestroyIdleUdpChannels();
    CleanDyingConnections();
    CleanDyingUdpChannels();
}

std::string xRelayLocalBindingService::OutputAudit() const {
    auto OS = std::ostringstream();
    OS << "xRelayLocalBindingService: " << endl;
    OS << "\tConnectionCount=" << Audit.ConnectionCount << endl;
    OS << "\tUdpChannelCount=" << Audit.UdpChannelCount << endl;
    OS << "\tDnsFutureCount=" << DnsFutureManager.GetActiveFutureCount() << endl;
    return OS.str();
}

bool xRelayLocalBindingService::CreateLocalDeviceList(const std::vector<xRelayLocalBindingOption> & OptionList) {
    X_RUNTIME_ASSERT(LocalDeviceList.empty());
    size_t IndexCount = 0;
    for (auto & O : OptionList) {
        auto DInfo = xRelayLocalDevice{
            .DeviceId      = MakeLocalDeviceId(IndexCount++),
            .BindAddress   = O.BindAddress,
            .ExportAddress = O.ExportAddress,
            .EnableTcp     = O.EnableTcp,
            .EnableUdp     = O.EnableUdp,
        };
        auto & E = LocalDeviceExportAddressMap[DInfo.ExportAddress];
        X_RUNTIME_ASSERT(!Steal(E, DInfo.DeviceId));
        LocalDeviceList.push_back(DInfo);
    }
    X_RUNTIME_ASSERT(LocalDeviceList.size() == LocalDeviceExportAddressMap.size());
    for (auto & D : LocalDeviceList) {
        AuditLogger->I("EnableDevice:\n\t%s", D.ToString().c_str());
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

void xRelayLocalBindingService::AcquireDevice(const xDeviceRequest & Request, xAcquireDeviceFuture & Future) {
    Future.SetReady();
    if (!(Request.Strategy & DSS_STATIC_EXPORT_ADDRESS)) {
        DEBUG_LOG();
        return;
    }
    auto Iter = LocalDeviceExportAddressMap.find(Request.Condition.ExportAddress);
    if (Iter == LocalDeviceExportAddressMap.end()) {
        DEBUG_LOG();
        return;
    }
    Future.Result = {
        .RelayServerId     = 0,
        .RelaySideDeviceId = Iter->second,
    };
    DEBUG_LOG();
    return;
}

void xRelayLocalBindingService::CreateConnection(uint64_t RelayServerId, uint64_t DeviceId, uint64_t PASideConnectionId, const std::string & TargetHostname, uint16_t TargetPort, xRelayCreateConnectionFuture & Future) {
    auto DnsFuture = DnsFutureManager.AcquireFuture();
    if (!DnsFuture) {
        Future.SetReady();
        return;
    }
    DnsFuture->RelayServerId      = RelayServerId;
    DnsFuture->DeviceId           = DeviceId;
    DnsFuture->PASideConnectionId = PASideConnectionId;

    if (!DnsService->ResolveDns(TargetHostname, *DnsFuture)) {
        DnsFutureManager.ReleaseFuture(DnsFuture);
        Future.SetReady();
        return;
    }
    if (DnsFuture->IsReady) {
        if (DnsFuture->Result) {
            auto & Result = *DnsFuture->Result;
            if (Result.A4) {
                CreateConnection(RelayServerId, DeviceId, PASideConnectionId, Result.A4, Future);
            } else if (Result.A6) {
                CreateConnection(RelayServerId, DeviceId, PASideConnectionId, Result.A6, Future);
            } else {
                DEBUG_LOG("no target address found");
                Future.SetReady();
            }
        } else {
            DEBUG_LOG("no target address found");
            Future.SetReady();
        }
        DnsFutureManager.ReleaseFuture(DnsFuture);
        return;
    }
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
    if (!Connection.Init(ServiceIoContext, TargetAddress, Device->BindAddress, this)) {
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
    if (!UdpChannel.Init(ServiceIoContext, Device->BindAddress, this)) {
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

void xRelayLocalBindingService::ProcessDnsResults() {
    auto & DnsReadyList = DnsFutureManager.GetReadyFutureList();
    while (auto P = static_cast<xRelayDnsResultFuture *>(DnsReadyList.PopHead())) {
        DnsFutureManager.ReleaseFuture(P);
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

void xRelayLocalBindingService::CleanDyingConnections() {
    while (auto P = static_cast<xRelayLocalDeviceConnection *>(ConnectionKillList.PopHead())) {
        P->Clean();
        LocalConnectionPool.Release(P->ConnectionId);
        --Audit.ConnectionCount;
    }
}

void xRelayLocalBindingService::CleanDyingUdpChannels() {
    while (auto P = static_cast<xRelayLocalDeviceUdpChannel *>(UdpChannelKillList.PopHead())) {
        P->Clean();
        LocalUdpChannelPool.Release(P->UdpChannelId);
        --Audit.UdpChannelCount;
    }
}

void xRelayLocalBindingService::CleanAllConnections() {
    ConnectionKillList.GrabListTail(ConnectionEstablishTimeoutList);
    ConnectionKillList.GrabListTail(ConnectionIdleTimeoutList);
    CleanDyingConnections();
}

void xRelayLocalBindingService::CleanAllUdpChannels() {
    UdpChannelKillList.GrabListTail(UdpChannelIdleTimeoutList);
    CleanDyingUdpChannels();
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
        Future->Result = Connection->ConnectionId;
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