#pragma once
#include <core/memory_pool.hpp>
#include <pp_common/_common.hpp>

using xPPMessage = uint32_t;

struct xMessageNode : xListNode {};

struct xLocalMessage : xMessageNode {
    xPPMessage     Message = 0;
    xel::xVariable P1;
    xel::xVariable P2;
};

extern xLocalMessage * AllocLocalMessage();
extern void            ReleaseLocalMessage(xLocalMessage * Message);
extern void            PushMessage(xLocalMessage * Message);
extern xLocalMessage * PopMessage();

constexpr const xPPMessage LMQ_CreateTcpConnection       = 0x01;
constexpr const xPPMessage LMQ_DestroyTcpConnection      = 0x02;
constexpr const xPPMessage LMQ_CreateTcpConnectionResult = 0x03;
