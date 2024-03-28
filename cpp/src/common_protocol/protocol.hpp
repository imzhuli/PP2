#pragma once
#include "../common/base.hpp"

static constexpr const xPacketCommandId CmdRespBase = 0x01'000;

static constexpr const xPacketCommandId Cmd_GeoQuery  = 0x01;
static constexpr const xPacketCommandId Cmd_HostQuery = 0x02;

static constexpr const xPacketCommandId Cmd_GeoQueryResp  = Cmd_GeoQuery + CmdRespBase;
static constexpr const xPacketCommandId Cmd_HostQueryResp = Cmd_HostQuery + CmdRespBase;

std::vector<ubyte> Encrypt(const void * Data, size_t DataSize, const std::string & AesKey);
std::vector<ubyte> Decrypt(const void * Data, size_t DataSize, const std::string & AesKey);
