#include "./server_register.hpp"

#include <pp_protocol/command.hpp>
#include <pp_protocol/internal/server_list.hpp>

bool xServerListRegister::Init(xel::xIoContext * IC) {
    if (!ClientWrapper.Init(IC)) {
        return false;
    }
    ClientWrapper.OnConnected    = Delegate(&xServerListRegister::OnConnected, this);
    ClientWrapper.OnDisconnected = []() { cerr << "Disconnected" << endl; };

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
    PostRegisterServiceInfo();
}

void xServerListRegister::PostRegisterServiceInfo() {
    auto Request        = xPP_UpdateServiceInfo();
    Request.ServiceType = MyServiceType;
    Request.ServiceInfo = MyServiceInfo;
    ClientWrapper.PostMessage(Cmd_RegisterService, 0, Request);

    Reset(MyServiceInfoDirty);
}
