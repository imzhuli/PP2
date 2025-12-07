#include "./server_register.hpp"

#include <pp_protocol/command.hpp>
#include <pp_protocol/internal/server_list.hpp>

bool xServerListRegister::Init(xel::xIoContext * IC) {
    if (!ClientWrapper.Init(IC)) {
        return false;
    }
    ClientWrapper.OnConnected    = Delegate(&xServerListRegister::OnConnected, this);
    ClientWrapper.OnDisconnected = []() {};
    ClientWrapper.OnPacket       = [this](xPacketCommandId CommandId, xPacketRequestId RequestId, ubyte * PayloadPtr, size_t PayloadSize) {
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
    if (MyServiceInfoDirty) {
        TryPostRegisterServiceInfo();
    }
}

void xServerListRegister::OnConnected() {
    TryPostRegisterServiceInfo();
}

void xServerListRegister::TryPostRegisterServiceInfo() {
    auto Request        = xPP_UpdateServiceInfo();
    Request.ServiceType = MyServiceInfo.ServerId ? MyServiceType : eServiceType::Unspecified;
    Request.ServiceInfo = MyServiceInfo;
    ClientWrapper.PostMessage(Cmd_RegisterService, 0, Request);
    Reset(MyServiceInfoDirty);
}
