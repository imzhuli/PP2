#pragma once
#include <pp_common/queue.hpp>

using xPPMessage = uint32_t;

struct xLocalMessage {
    xPPMessage Message = 0;
    xel::xVariable P1;
    xel::xVariable P2;
};

using xLocalMessageQueue = xFixedSizeArrayQueue<xLocalMessage>;

constexpr const xPPMessage LMQ_CreateTcpConnection        = 0x01;
constexpr const xPPMessage LMQ_DestroyTcpConnection       = 0x02;
constexpr const xPPMessage LMQ_CreateTcpConnectionResult  = 0x03;

