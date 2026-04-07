#include <pp_common/service_runtime.hpp>
#include <pp_protocol/command.hpp>
#include <pp_protocol/p_echo.hpp>

static auto DsAddress = xNetAddress::Parse("127.0.0.1:9001");
static auto DsClient  = xTcpClient();

bool OnClientPacket(xPacketCommandId CommandId, xPacketRequestId RequestId, ubyte * PayloadPtr, size_t PayloadSize) {
    if (CommandId != Cmd_EchoTest) {
        cout << "invalid request" << endl;
        return true;
    }
    auto Msg = xPP_EchoTest();
    if (!Msg.Deserialize(PayloadPtr, PayloadSize)) {
        cout << "invalid data format" << endl;
        return true;
    }
    cout << "RequestId:" << RequestId << ", Data=" << Msg.Data << endl;

    DsClient.PostMessage(Cmd_EchoTestResp, RequestId, Msg);
    return true;
}

int main(int argc, char ** argv) {
    X_VAR xServiceEnvironmentGuard(argc, argv, ServiceConsoleLogger);
    X_RESOURCE_GUARD_ASSERTED(DsClient, ServiceIoContext, DsAddress);
    DsClient.OnServerPacket = OnClientPacket;

    while (ServiceRunState) {
        ServiceUpdateOnce(DsClient);
    }
}