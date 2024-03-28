#pragma once
#include "../common/base.hpp"
#include "./protocol.hpp"

#include <google/protobuf/message.h>

using xPbMessage = ::google::protobuf::Message;

[[no_discard]] size_t      PbWritePacket(xPacketCommandId CmdId, xPacketRequestId RequestId, void * Buffer, size_t BufferSize, xPbMessage & Message);
[[no_discard]] std::string PbToJson(const xPbMessage & PbMessage);
[[no_discard]] std::string PbToJsonDetailed(const xPbMessage & PbMessage);
