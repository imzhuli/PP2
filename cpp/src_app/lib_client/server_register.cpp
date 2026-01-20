#include "./server_register.hpp"

#include <pp_protocol/command.hpp>
#include <pp_protocol/internal/server_list.hpp>

bool xServerListRegister::Init(xel::xIoContext * IC) {
    if (!ClientWrapper.Init(IC)) {
        return false;
    }
    ClientWrapper.OnServerConnected    = Delegate(&xServerListRegister::OnConnected, this);
    ClientWrapper.OnServerDisconnected = []() {};
    ClientWrapper.OnServerPacket       = [this](xPacketCommandId CommandId, xPacketRequestId RequestId, ubyte * PayloadPtr, size_t PayloadSize) {
        if (CommandId != Cmd_RegisterServiceResp) {
            OnError();
            return false;
        }
        auto Resp = xPP_UpdateServiceInfoResp();
        if (!Resp.Deserialize(PayloadPtr, PayloadSize)) {
            OnError();
            return false;
        }

        if (!Resp.Accepted) {
            OnError();
            return false;
        }
        return true;
    };

    return true;
}

void xServerListRegister::Clean() {
    ClientWrapper.Clean();
}

void xServerListRegister::Tick(uint64_t NowMS) {
    ClientWrapper.Tick(NowMS);
    if (Steal(MyServiceInfoDirty)) {
        PostRegisterServiceInfo();
    }
}

void xServerListRegister::OnConnected() {
    Reset(MyServiceInfoDirty);
    if (MyServiceType != eServiceType::Unspecified && MyServiceInfo.ServerId) {
        PostRegisterServiceInfo();
    }
}

void xServerListRegister::PostRegisterServiceInfo() {
    auto Request        = xPP_UpdateServiceInfo();
    Request.ServiceType = MyServiceInfo.ServerId ? MyServiceType : eServiceType::Unspecified;
    Request.ServiceInfo = MyServiceInfo;
    ClientWrapper.PostMessage(Cmd_RegisterService, 0, Request);
}
