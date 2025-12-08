#include "./relay.hpp"

#include <pp_common/service_runtime.hpp>
#include <pp_protocol/command.hpp>

static constexpr const size_t MAX_CONNECTION_PER_SERVICE = 2'0000;

static xTcpService InputService;   // observer
static xTcpService OutputService;  // producer

static auto Connections = std::vector<std::unique_ptr<xTcpServiceClientConnectionHandle>>();

static void DispatchData(const void * DataPtr, size_t DataSize) {
    // DEBUG_LOG("Dispatching data:\n%s", HexShow(DataPtr, DataSize).c_str());
    for (auto & H : Connections) {
        DEBUG_LOG("ToConnection:%" PRIx64 "", H->GetConnectionId());
        H->PostData(DataPtr, DataSize);
    }
}

void InitRelayService(const xNetAddress & InputAddress, const xNetAddress & OutputAddress) {
    RuntimeAssert(InputService.Init(ServiceIoContext, InputAddress, MAX_CONNECTION_PER_SERVICE));
    InputService.OnClientPacket = [](const xTcpServiceClientConnectionHandle Handle, xPacketCommandId CommandId, xPacketRequestId RequestId, ubyte * PayloadPtr,
                                     size_t PayloadSize) {
        switch (CommandId) {
            case Cmd_DSR_DS_DeviceUpdate: {
                ubyte  B[MaxPacketSize];
                size_t RS = BuildPacket(B, CommandId, 0, PayloadPtr, PayloadSize);
                assert(RS);
                DispatchData(B, RS);
            } break;
            default: {
                DEBUG_LOG("invalid command");
                Pass();
            } break;
        }
        return true;
    };

    RuntimeAssert(OutputService.Init(ServiceIoContext, OutputAddress, MAX_CONNECTION_PER_SERVICE));
    OutputService.OnClientConnected = [](const xTcpServiceClientConnectionHandle & H) { Connections.push_back(std::make_unique<xTcpServiceClientConnectionHandle>(H)); };
    OutputService.OnClientClean     = [](const xTcpServiceClientConnectionHandle & H) {
        auto I = Connections.begin();
        auto E = Connections.end();
        auto T = H.operator->();
        while (I != E) {
            if (I->get()->operator->() == T) {
                Connections.erase(I);
                break;
            }
            ++I;
        }
    };
}

void CleanRelayService() {
    OutputService.Clean();
    InputService.Clean();
}

void RelayTicker(uint64_t NowMS) {
    TickAll(NowMS, InputService, OutputService);
}
