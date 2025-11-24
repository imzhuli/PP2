#include "../lib_util/service_bootstrap.hpp"
#include "./config.hpp"
#include "./income.hpp"
#include "./output.hpp"

#include <pp_common/_.hpp>
#include <pp_common/service_runtime.hpp>
#include <pp_protocol/command.hpp>

using namespace xel;

class xObserverService;
class xProducerService;

static std::vector<std::unique_ptr<xTcpServiceClientConnectionHandle>> Connections;
static xTcpService                                                     OS;  // observer
static xTcpService                                                     PS;  // producer

static void DispatchData(const void * DataPtr, size_t DataSize) {
    // DEBUG_LOG("Dispatching data:\n%s", HexShow(DataPtr, DataSize).c_str());
    for (auto & H : Connections) {
        DEBUG_LOG("ToConnection:%" PRIx64 "", H->GetConnectionId());
        H->PostData(DataPtr, DataSize);
    }
}

static auto ServiceGuard = xScopeGuard(
    [] {
        OS.OnClientConnected = [](const xTcpServiceClientConnectionHandle & H) { Connections.push_back(std::make_unique<xTcpServiceClientConnectionHandle>(H)); };
        OS.OnClientClose     = [](const xTcpServiceClientConnectionHandle & H) {
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
        PS.OnClientPacket = [](const xTcpServiceClientConnectionHandle Handle, xPacketCommandId CommandId, xPacketRequestId RequestId, ubyte * PayloadPtr, size_t PayloadSize) {
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
    },
    xPass()
);

int main(int argc, char ** argv) {
    X_VAR xServiceRuntimeEnvGuard(argc, argv);
    LoadConfig();

    while (true) {
        ServiceUpdateOnce();
    }

    return 0;
}
