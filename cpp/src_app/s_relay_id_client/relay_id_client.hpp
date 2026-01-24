#pragma once
#include <pp_common/_.hpp>

class xRelayIdClient final : xel::xNonCopyable {
public:
    bool Init(const xNetAddress & ServerAddress, uint64_t PreviousRelayServerId = 0);
    void Clean();
    void Tick(uint64_t NowMS);

    using xOnRelayIdUpdated            = std::function<void(uint64_t NewRelayId)>;
    xOnRelayIdUpdated OnRelayIdUpdated = Noop<>;

private:
    void OnServerConnected();
    bool OnServerPacket(xPacketCommandId CommandId, xPacketRequestId RequestId, ubyte * PayloadPtr, size_t PayloadSize);

private:
    xClient  Client;
    uint64_t PreviousRelayServerId = 0;
};