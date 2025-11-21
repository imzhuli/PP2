#pragma once
#include <pp_common/_.hpp>

extern uint64_t LoadLocalServerId(const std::string & LocalServerIdFilename);
extern void     DumpLocalServerId(const std::string & LocalServerIdFilename, uint64_t LocalServerId);

class xServerIdClient final {
public:
    bool Init(xIoContext * ICP, xNetAddress TargetAddress, uint64_t FirstTryServerId = 0);
    bool Init(xIoContext * ICP, xNetAddress TargetAddress, const std::string & LocalServerIdFilename);
    void Clean();
    void Tick(uint64_t NowMS);

    uint64_t GetLocalServerId() const { return LocalServerId; }

    std::function<void(uint64_t)> OnServerIdUpdateCallback = Noop<>;

private:
    void OnServerConnected();
    bool OnServerPacket(xPacketCommandId CommandId, xPacketRequestId RequestId, ubyte * PayloadPtr, size_t PayloadSize);
    void OnServerIdUpdated(uint64_t NewServerId) { OnServerIdUpdateCallback(NewServerId); }

private:
    xClientWrapper ClientWrapper;
    uint64_t       LocalServerId      = 0;
    bool           LocalServerIdDirty = false;
    std::string    LocalServerIdFilename;
};
