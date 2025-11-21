#pragma once
#include <pp_common/_.hpp>

// static constexpr const xPacketCommandId CmdRespBase = xel::MaxDispatchableCommandIdCount;

// static constexpr const xPacketCommandId Cmd_GeoQuery            = 0x01;
// static constexpr const xPacketCommandId Cmd_HostQuery           = 0x02;
// static constexpr const xPacketCommandId Cmd_GetTerminalWithAuth = 0x03;
// static constexpr const xPacketCommandId Cmd_ReportAccountUsage  = 0x04;
// static constexpr const xPacketCommandId Cmd_ProxyClientAuth     = 0x05;

// static constexpr const xPacketCommandId Cmd_GeoQueryResp            = CmdRespBase + Cmd_GeoQuery;
// static constexpr const xPacketCommandId Cmd_HostQueryResp           = CmdRespBase + Cmd_HostQuery;
// static constexpr const xPacketCommandId Cmd_GetTerminalWithAuthResp = CmdRespBase + Cmd_GetTerminalWithAuth;
// static constexpr const xPacketCommandId Cmd_ReportAccountUsageResp  = CmdRespBase + Cmd_ReportAccountUsage;  // not used
// static constexpr const xPacketCommandId Cmd_ProxyClientAuthResp     = CmdRespBase + Cmd_ProxyClientAuth;

// non dispatchable or direct command
static constexpr const xPacketCommandId CmdProxyBase = 0x02'000;
// static constexpr const xPacketCommandId Cmd_CreateConnection         = CmdProxyBase + 0x01;
// static constexpr const xPacketCommandId Cmd_CreateConnectionResp     = CmdProxyBase + 0x02;
// static constexpr const xPacketCommandId Cmd_CloseConnection          = CmdProxyBase + 0x03;
// static constexpr const xPacketCommandId Cmd_PostProxyToRelayData     = CmdProxyBase + 0x04;
// static constexpr const xPacketCommandId Cmd_PostRelayToProxyData     = CmdProxyBase + 0x05;
// static constexpr const xPacketCommandId Cmd_CreateUdpAssociation     = CmdProxyBase + 0x06;
// static constexpr const xPacketCommandId Cmd_CreateUdpAssociationResp = CmdProxyBase + 0x07;
// static constexpr const xPacketCommandId Cmd_CloseUdpAssociation      = CmdProxyBase + 0x08;
// static constexpr const xPacketCommandId Cmd_PostProxyToRelayUdpData  = CmdProxyBase + 0x09;
// static constexpr const xPacketCommandId Cmd_PostRelayToProxyUdpData  = CmdProxyBase + 0x0A;

// management
static constexpr const xPacketCommandId Cmd_ManagementBase       = 0x02'100;
static constexpr const xPacketCommandId Cmd_PostError            = Cmd_ManagementBase + 0x01;
static constexpr const xPacketCommandId Cmd_PostErrorResp        = Cmd_ManagementBase + 0x02;
static constexpr const xPacketCommandId Cmd_PostTerminalAudit    = Cmd_ManagementBase + 0x03;
static constexpr const xPacketCommandId Cmd_PostRelayServerAudit = Cmd_ManagementBase + 0x04;

// version 2024 from here

// backend base:
static constexpr const xPacketCommandId Cmd_BackendBase           = 0x04'000;
static constexpr const xPacketCommandId Cmd_AuditTerminalInfo     = Cmd_BackendBase + 0x01;  // 旧版, 弃用
static constexpr const xPacketCommandId Cmd_AuditTerminalInfoResp = Cmd_BackendBase + 0x02;  // 一般不处理返回

static constexpr const xPacketCommandId Cmd_AuditUsageByAuditId         = Cmd_BackendBase + 0x03;  //
static constexpr const xPacketCommandId Cmd_AuditUsageByAuditIdResp     = Cmd_BackendBase + 0x04;  // 一般不处理返回
static constexpr const xPacketCommandId Cmd_AuditTerminalInfo2          = Cmd_BackendBase + 0x05;  // 2025-03-09 新增, 无返回
static constexpr const xPacketCommandId Cmd_DeviceRelayServerInfo2      = Cmd_BackendBase + 0x06;  // 2025-03-09 新增, 无返回
static constexpr const xPacketCommandId Cmd_DeviceRelayServerInfoSingle = Cmd_BackendBase + 0x07;  // 2025-05-28 新增, 无返回
static constexpr const xPacketCommandId Cmd_AuditThirdAccountUsage      = Cmd_BackendBase + 0x08;  // 2025-05-28 新增, 无返回

static constexpr const xPacketCommandId Cmd_BackendChallenge          = Cmd_BackendBase + 0x101;
static constexpr const xPacketCommandId Cmd_BackendChallengeResp      = Cmd_BackendBase + 0x102;
static constexpr const xPacketCommandId Cmd_BackendAuthByUserPass     = Cmd_BackendBase + 0x201;
static constexpr const xPacketCommandId Cmd_BackendAuthByUserPassResp = Cmd_BackendBase + 0x202;

// server management:
static constexpr const xPacketCommandId Cmd_ServerManagementBase    = 0x05'000;
static constexpr const xPacketCommandId Cmd_QueryRunningState       = Cmd_ServerManagementBase + 1;
static constexpr const xPacketCommandId Cmd_QueryRunningStateResp   = Cmd_ServerManagementBase + 2;
static constexpr const xPacketCommandId Cmd_RegisterDnsServer       = Cmd_ServerManagementBase + 3;  // get dns server group id from config center
static constexpr const xPacketCommandId Cmd_RegisterDnsServerResp   = Cmd_ServerManagementBase + 4;
static constexpr const xPacketCommandId Cmd_CheckinDnsServer        = Cmd_ServerManagementBase + 5;  // check into dispatcher with group id from config center
static constexpr const xPacketCommandId Cmd_CheckinDnsServerResp    = Cmd_ServerManagementBase + 6;
static constexpr const xPacketCommandId Cmd_EnableDnsDispatcher     = Cmd_ServerManagementBase + 7;
static constexpr const xPacketCommandId Cmd_EnableDnsDispatcherResp = Cmd_ServerManagementBase + 8;

static constexpr const xPacketCommandId Cmd_AcquireServerId         = Cmd_ServerManagementBase + 0x101;
static constexpr const xPacketCommandId Cmd_AcquireServerIdResp     = Cmd_ServerManagementBase + 0x102;
static constexpr const xPacketCommandId Cmd_RegisterService         = Cmd_ServerManagementBase + 0x103;
static constexpr const xPacketCommandId Cmd_RegisterServiceResp     = Cmd_ServerManagementBase + 0x104;
static constexpr const xPacketCommandId Cmd_DownloadServiceList     = Cmd_ServerManagementBase + 0x105;
static constexpr const xPacketCommandId Cmd_DownloadServiceListResp = Cmd_ServerManagementBase + 0x106;

///////////////////////// PPPro server list
static constexpr const xPacketCommandId Cmd_RegisterAuthCacheServer                        = Cmd_ServerManagementBase + 0x201;
static constexpr const xPacketCommandId Cmd_RegisterAuditDeviceServer                      = Cmd_ServerManagementBase + 0x202;
static constexpr const xPacketCommandId Cmd_RegisterAuditAccountServer                     = Cmd_ServerManagementBase + 0x203;
static constexpr const xPacketCommandId Cmd_RegisterDeviceStateRelayServer                 = Cmd_ServerManagementBase + 0x204;
static constexpr const xPacketCommandId Cmd_RegisterRelayInfoDispatcherServer              = Cmd_ServerManagementBase + 0x205;
static constexpr const xPacketCommandId Cmd_RegisterDeviceSelectorDispatcherServer         = Cmd_ServerManagementBase + 0x206;
static constexpr const xPacketCommandId Cmd_RegisterDeviceSelector                         = Cmd_ServerManagementBase + 0x207;
static constexpr const xPacketCommandId Cmd_RegisterAuditTargetServer                      = Cmd_ServerManagementBase + 0x208;
static constexpr const xPacketCommandId Cmd_RegisterServerResp                             = Cmd_ServerManagementBase + 0x2FF;
static constexpr const xPacketCommandId Cmd_DownloadAuthCacheServerList                    = Cmd_ServerManagementBase + 0x301;
static constexpr const xPacketCommandId Cmd_DownloadAuthCacheServerListResp                = Cmd_ServerManagementBase + 0x302;
static constexpr const xPacketCommandId Cmd_DownloadAuditDeviceServerList                  = Cmd_ServerManagementBase + 0x303;
static constexpr const xPacketCommandId Cmd_DownloadAuditDeviceServerListResp              = Cmd_ServerManagementBase + 0x304;
static constexpr const xPacketCommandId Cmd_DownloadAuditAccountServerList                 = Cmd_ServerManagementBase + 0x305;
static constexpr const xPacketCommandId Cmd_DownloadAuditAccountServerListResp             = Cmd_ServerManagementBase + 0x306;
static constexpr const xPacketCommandId Cmd_DownloadDeviceStateRelayServerList             = Cmd_ServerManagementBase + 0x307;
static constexpr const xPacketCommandId Cmd_DownloadDeviceStateRelayServerListResp         = Cmd_ServerManagementBase + 0x308;
static constexpr const xPacketCommandId Cmd_DownloadBackendServerList                      = Cmd_ServerManagementBase + 0x309;
static constexpr const xPacketCommandId Cmd_DownloadBackendServerListResp                  = Cmd_ServerManagementBase + 0x30A;
static constexpr const xPacketCommandId Cmd_DownloadRelayInfoDispatcherServer              = Cmd_ServerManagementBase + 0x30B;
static constexpr const xPacketCommandId Cmd_DownloadRelayInfoDispatcherServerResp          = Cmd_ServerManagementBase + 0x30C;
static constexpr const xPacketCommandId Cmd_DownloadDeviceSelectorDispatcherServerList     = Cmd_ServerManagementBase + 0x30D;
static constexpr const xPacketCommandId Cmd_DownloadDeviceSelectorDispatcherServerListResp = Cmd_ServerManagementBase + 0x30E;
static constexpr const xPacketCommandId Cmd_DownloadAuditTargetServerList                  = Cmd_ServerManagementBase + 0x30F;
static constexpr const xPacketCommandId Cmd_DownloadAuditTargetServerListResp              = Cmd_ServerManagementBase + 0x310;

///////////////////////// PP2 server list

static constexpr const xPacketCommandId Cmd_RelayServerHeartBeat      = Cmd_ServerManagementBase + 0x401;
static constexpr const xPacketCommandId Cmd_RegisterRelayInfoObserver = Cmd_ServerManagementBase + 0x402;
static constexpr const xPacketCommandId Cmd_BroadcastRelayInfo        = Cmd_ServerManagementBase + 0x403;
static constexpr const xPacketCommandId Cmd_BroadcastRelayOffline     = Cmd_ServerManagementBase + 0x404;

static constexpr const xPacketCommandId Cmd_AuditAccountUsage = Cmd_ServerManagementBase + 0x501;
static constexpr const xPacketCommandId Cmd_AuditTarget       = Cmd_ServerManagementBase + 0x502;

// pa-relay:
static constexpr const xPacketCommandId Cmd_PA_RL_Base                  = 0x06'000;
static constexpr const xPacketCommandId Cmd_PA_RL_Challenge             = Cmd_PA_RL_Base + 0x01;
static constexpr const xPacketCommandId Cmd_PA_RL_ChallengeResp         = Cmd_PA_RL_Base + 0x02;
static constexpr const xPacketCommandId Cmd_PA_RL_CreateConnection      = Cmd_PA_RL_Base + 0x03;
static constexpr const xPacketCommandId Cmd_PA_RL_DestroyConnection     = Cmd_PA_RL_Base + 0x04;
static constexpr const xPacketCommandId Cmd_PA_RL_NotifyConnectionState = Cmd_PA_RL_Base + 0x05;
static constexpr const xPacketCommandId Cmd_PA_RL_PostData              = Cmd_PA_RL_Base + 0x06;
static constexpr const xPacketCommandId Cmd_PA_RL_ProxyClientNotify     = Cmd_PA_RL_Base + 0x09;  //
static constexpr const xPacketCommandId Cmd_PA_RL_CreateUdpBinding      = Cmd_PA_RL_Base + 0x0A;  //
static constexpr const xPacketCommandId Cmd_PA_RL_DestroyUdpBinding     = Cmd_PA_RL_Base + 0x0B;  //
static constexpr const xPacketCommandId Cmd_PA_RL_KeepAliveUdpBinding   = Cmd_PA_RL_Base + 0x0C;  //
static constexpr const xPacketCommandId Cmd_PA_RL_PostUdpData           = Cmd_PA_RL_Base + 0x0D;  //

// cc-device:
static constexpr const xPacketCommandId Cmd_DV_CC_Base          = 0x07'000;
static constexpr const xPacketCommandId Cmd_DV_CC_Challenge     = Cmd_DV_CC_Base + 0x01;
static constexpr const xPacketCommandId Cmd_DV_CC_ChallengeResp = Cmd_DV_CC_Base + 0x02;

// device-relay:
static constexpr const xPacketCommandId Cmd_DV_RL_Base                 = 0x08'000;
static constexpr const xPacketCommandId Cmd_DV_RL_Handshake            = Cmd_DV_RL_Base + 0x01;
static constexpr const xPacketCommandId Cmd_DV_RL_HandshakeResp        = Cmd_DV_RL_Base + 0x02;
static constexpr const xPacketCommandId Cmd_DV_RL_ConfigDevice         = Cmd_DV_RL_Base + 0x03;
static constexpr const xPacketCommandId Cmd_DV_RL_InitDataStreamResp   = Cmd_DV_RL_Base + 0x04;  // deprecated
static constexpr const xPacketCommandId Cmd_DV_RL_CreateConnection     = Cmd_DV_RL_Base + 0x05;
static constexpr const xPacketCommandId Cmd_DV_RL_CreateConnectionHost = Cmd_DV_RL_Base + 0x06;
static constexpr const xPacketCommandId Cmd_DV_RL_CreateConnectionResp = Cmd_DV_RL_Base + 0x07;
static constexpr const xPacketCommandId Cmd_DV_RL_PostConnectionData   = Cmd_DV_RL_Base + 0x08;
static constexpr const xPacketCommandId Cmd_DV_RL_DestroyConnection    = Cmd_DV_RL_Base + 0x09;
static constexpr const xPacketCommandId Cmd_DV_RL_KeepAliveConnection  = Cmd_DV_RL_Base + 0x0A;

static constexpr const xPacketCommandId Cmd_DV_RL_CreateUdpChannel     = Cmd_DV_RL_Base + 0x10;
static constexpr const xPacketCommandId Cmd_DV_RL_DestroyUdpChannel    = Cmd_DV_RL_Base + 0x11;
static constexpr const xPacketCommandId Cmd_DV_RL_PostUdpChannelData   = Cmd_DV_RL_Base + 0x12;
static constexpr const xPacketCommandId Cmd_DV_RL_CreateUdpChannelResp = Cmd_DV_RL_Base + 0x13;
static constexpr const xPacketCommandId Cmd_DV_RL_KeepAliveUdpChannel  = Cmd_DV_RL_Base + 0x14;

static constexpr const xPacketCommandId Cmd_DV_RL_DnsQuery     = Cmd_DV_RL_Base + 0x20;
static constexpr const xPacketCommandId Cmd_DV_RL_DnsQueryResp = Cmd_DV_RL_Base + 0x21;

static constexpr const xPacketCommandId Cmd_DV_RL_AddressChallenge = Cmd_DV_RL_Base + 0x30;
static constexpr const xPacketCommandId Cmd_DV_RL_AddressPush      = Cmd_DV_RL_Base + 0x31;

// device-state-relay(dsr) / device-selector
static constexpr const xPacketCommandId Cmd_DSR_DS_Base         = 0x09'000;
static constexpr const xPacketCommandId Cmd_DSR_DS_DeviceUpdate = Cmd_DSR_DS_Base + 0x01;

// device-selector
static constexpr const xPacketCommandId Cmd_DeviceSelector_Base              = 0x0D'000;
static constexpr const xPacketCommandId Cmd_DeviceSelector_AcquireDevice     = Cmd_DeviceSelector_Base + 0x01;
static constexpr const xPacketCommandId Cmd_DeviceSelector_AcquireDeviceResp = Cmd_DeviceSelector_Base + 0x02;

// auth-service
static constexpr const xPacketCommandId Cmd_AuthService_Base               = 0x0E'000;
static constexpr const xPacketCommandId Cmd_AuthService_QueryAuthCache     = Cmd_AuthService_Base + 0x01;
static constexpr const xPacketCommandId Cmd_AuthService_QueryAuthCacheResp = Cmd_AuthService_Base + 0x02;

std::vector<ubyte> Encrypt(const void * Data, size_t DataSize, const std::string & AesKey);
std::vector<ubyte> Decrypt(const void * Data, size_t DataSize, const std::string & AesKey);
