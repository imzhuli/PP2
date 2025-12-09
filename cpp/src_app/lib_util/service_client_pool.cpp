#include "./service_client_pool.hpp"

#include <pp_common/service_runtime.hpp>

bool xPPClientPool::Init(size_t MaxConnectionCount) {
    if (!ClientPool.Init(ServiceIoContext, MaxConnectionCount)) {
        return false;
    }
    ClientPool.OnTargetConnected = Delegate(&xPPClientPool::OnTargetConnected, this);
    ClientPool.OnTargetClose     = Delegate(&xPPClientPool::OnTargetClose, this);
    ClientPool.OnTargetPacket    = Delegate(&xPPClientPool::OnPacketCallback, this);

    TmpAdd.reserve(MaxConnectionCount);
    TmpRem.reserve(MaxConnectionCount);
    AuditLogger->I("xPPClientPool inited, this=%p", this);

    return true;
}

void xPPClientPool::Clean() {
    ClientPool.Clean();
    Reset(SortedServerList);

    AuditLogger->I("xPPClientPool cleaned, this=%p", this);
}

void xPPClientPool::Tick(uint64_t NowMS) {
    ClientPool.Tick(NowMS);
}

void xPPClientPool::UpdateServerList(const std::vector<xServerInfo> & ServerInfoList) {
    size_t IOld = 0;
    size_t INew = 0;
    while (IOld < SortedServerList.size() && INew < ServerInfoList.size()) {
        auto & O = SortedServerList[IOld];
        auto & N = ServerInfoList[INew];
        if (O.ServerId < N.ServerId) {
            TmpRem.push_back(O);
            ++IOld;
            continue;
        }
        if (N.ServerId < O.ServerId) {
            TmpAdd.push_back(N);
            ++INew;
            continue;
        }
        ++IOld;
        ++INew;
    }
    for (; INew < ServerInfoList.size(); ++INew) {
        TmpAdd.push_back(ServerInfoList[INew]);
    }
    for (; IOld < SortedServerList.size(); ++IOld) {
        TmpRem.push_back(SortedServerList[IOld]);
    }
    for (const auto & A : TmpAdd) {
        AddServer(A);
    }
    for (const auto & R : TmpRem) {
        RemoveServer(R);
    }
    TmpAdd.clear();
    TmpRem.clear();
}

void xPPClientPool::AddServer(const xServerInfo & ServerInfo) {
    auto Temp = xInternalServerInfo{
        ServerInfo.ServerId,
        0,
    };
    auto LB = std::lower_bound(SortedServerList.begin(), SortedServerList.end(), Temp, xInternalServerInfo::LessByServerId);
    SERVICE_RUNTIME_ASSERT(LB == SortedServerList.end() || LB->ServerId != Temp.ServerId);
    SERVICE_RUNTIME_ASSERT(Temp.LocalServerId = ClientPool.AddServer(ServerInfo.Address));
    auto NewIter = SortedServerList.insert(LB, Temp);
    AuditLogger->I("%p AddServer: id=%" PRIu64 ", connectionId=%" PRIu64 ", Address=%s", this, NewIter->ServerId, NewIter->LocalServerId, ServerInfo.Address.ToString().c_str());
}

void xPPClientPool::RemoveServer(const xInternalServerInfo & TargetServerInfo) {
    auto LB = std::lower_bound(SortedServerList.begin(), SortedServerList.end(), TargetServerInfo, xInternalServerInfo::LessByServerId);
    SERVICE_RUNTIME_ASSERT(LB != SortedServerList.end() && *LB == TargetServerInfo);
    ClientPool.RemoveServer(LB->LocalServerId);
    SortedServerList.erase(LB);
    AuditLogger->I("%p RemoveServer: id=%" PRIu64 ", connectionId=%" PRIu64 "", this, TargetServerInfo.ServerId, TargetServerInfo.LocalServerId);
}

void xPPClientPool::PostMessageByConnectionId(uint64_t ConnectionId, xPacketCommandId CmdId, xPacketRequestId RequestId, xBinaryMessage & Message) {
    ClientPool.PostMessage(ConnectionId, CmdId, RequestId, Message);
}

void xPPClientPool::PostMessageByHash(uint64_t Hash, xPacketCommandId CmdId, xPacketRequestId RequestId, xBinaryMessage & Message) {
    if (SortedServerList.empty()) {
        return;
    }
    auto   Index     = Hash % SortedServerList.size();
    auto & ServerRef = SortedServerList[Index];
    ClientPool.PostMessage(ServerRef.LocalServerId, CmdId, RequestId, Message);
}

void xPPClientPool::PostMessage(xPacketCommandId CmdId, xPacketRequestId RequestId, xBinaryMessage & Message) {
    ClientPool.PostMessage(CmdId, RequestId, Message);
}

void xPPClientPool::OnTargetConnected(const xClientPoolConnectionHandle & CC) {
    AuditLogger->I("OnTargetConnected: Id=" PRIu64 ", Address=%s", CC.GetConnectionId(), CC.GetTargetAddress().ToString().c_str());
}

void xPPClientPool::OnTargetClose(const xClientPoolConnectionHandle & CC) {
    AuditLogger->I("OnTargetClosed: Id=" PRIu64 ", Address=%s", CC.GetConnectionId(), CC.GetTargetAddress().ToString().c_str());
}

bool xPPClientPool::OnPacketCallback(const xClientPoolConnectionHandle & CC, xPacketCommandId CommandId, xPacketRequestId RequestId, ubyte * PayloadPtr, size_t PayloadSize) {
    return OnPacket(CC, CommandId, RequestId, PayloadPtr, PayloadSize);
}
