#include <pp_common/service_runtime.hpp>
#include <pp_protocol/command.hpp>
#include <pp_protocol/p_echo.hpp>

static auto DsAddress = xNetAddress::Parse("127.0.0.1:9000");
static auto DsClient  = xTcpClient();

static auto HelloTicker = xTimer();
static void SayHello() {
    if (!HelloTicker.TestAndTag(1s)) {
        return;
    }
    auto Msg = xPP_EchoTest();
    auto Now = xel::GetUnixTimestamp();
    Msg.Data = std::to_string(Now);
    cout << "Hello" << endl;
    DsClient.PostMessage(Cmd_EchoTest, Now, Msg);
}

bool OnServerPacket(xPacketCommandId CommandId, xPacketRequestId RequestId, ubyte * PayloadPtr, size_t PayloadSize) {
    if (CommandId != Cmd_EchoTestResp) {
        cout << "invalid request" << endl;
        return true;
    }
    auto Msg = xPP_EchoTestResp();
    if (!Msg.Deserialize(PayloadPtr, PayloadSize)) {
        cout << "invalid data format" << endl;
        return true;
    }
    cout << "RequestId:" << RequestId << ", Data=%s" << Msg.Data << endl;
    return true;
}

int main(int argc, char ** argv) {
    X_VAR xServiceEnvironmentGuard(argc, argv, ServiceConsoleLogger);
    X_RESOURCE_GUARD_ASSERTED(DsClient, ServiceIoContext, DsAddress);
    DsClient.OnServerPacket = OnServerPacket;

    while (ServiceRunState) {
        ServiceUpdateOnce(DsClient);
        SayHello();
    }
}