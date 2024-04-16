#pragma once
#include "../common/base.hpp"

static constexpr const xPacketCommandId CmdRespBase = MaxDispatchableCommandIdCount;

static constexpr const xPacketCommandId Cmd_GeoQuery            = 0x01;
static constexpr const xPacketCommandId Cmd_HostQuery           = 0x02;
static constexpr const xPacketCommandId Cmd_GetTerminalWithAuth = 0x03;
static constexpr const xPacketCommandId Cmd_ReportAccountUsage  = 0x04;
static constexpr const xPacketCommandId Cmd_ProxyClientAuth     = 0x05;

static constexpr const xPacketCommandId Cmd_GeoQueryResp            = CmdRespBase + Cmd_GeoQuery;
static constexpr const xPacketCommandId Cmd_HostQueryResp           = CmdRespBase + Cmd_HostQuery;
static constexpr const xPacketCommandId Cmd_GetTerminalWithAuthResp = CmdRespBase + Cmd_GetTerminalWithAuth;
static constexpr const xPacketCommandId Cmd_ReportAccountUsageResp  = CmdRespBase + Cmd_ReportAccountUsage;  // not used
static constexpr const xPacketCommandId Cmd_ProxyClientAuthResp     = CmdRespBase + Cmd_ProxyClientAuth;

// non dispatchable or direct command
static constexpr const xPacketCommandId CmdNDBase                    = 0x02'000;
static constexpr const xPacketCommandId Cmd_CreateConnection         = CmdNDBase + 0x01;
static constexpr const xPacketCommandId Cmd_CreateConnectionResp     = CmdNDBase + 0x02;
static constexpr const xPacketCommandId Cmd_CloseConnection          = CmdNDBase + 0x03;
static constexpr const xPacketCommandId Cmd_PostProxyToRelayData     = CmdNDBase + 0x04;
static constexpr const xPacketCommandId Cmd_PostRelayToProxyData     = CmdNDBase + 0x05;
static constexpr const xPacketCommandId Cmd_CreateUdpAssociation     = CmdNDBase + 0x06;
static constexpr const xPacketCommandId Cmd_CreateUdpAssociationResp = CmdNDBase + 0x07;
static constexpr const xPacketCommandId Cmd_CloseUdpAssociation      = CmdNDBase + 0x08;
static constexpr const xPacketCommandId Cmd_PostProxyToRelayUdpData  = CmdNDBase + 0x09;
static constexpr const xPacketCommandId Cmd_PostRelayToProxyUdpData  = CmdNDBase + 0x0A;

std::vector<ubyte> Encrypt(const void * Data, size_t DataSize, const std::string & AesKey);
std::vector<ubyte> Decrypt(const void * Data, size_t DataSize, const std::string & AesKey);
