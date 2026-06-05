#include "./p_audit.hpp"

////////////

void xPP_BlockAccount::SerializeMembers() {
    W(AuthId);
    W(StartTimestampMS);
    W(PeriodMS);
    W(Reason);
}

void xPP_BlockAccount::DeserializeMembers() {
    R(AuthId);
    R(StartTimestampMS);
    R(PeriodMS);
    R(Reason);
}

///////////////////

void xPP_AccountUsage::SerializeMembers() {
    W(AuthId);
    W(StartTimestampMS);
    W(PeriodMS);

    W(TotalTcpBytesFromClient);
    W(TotalTcpBytesToClient);
    W(TotalUdpBytesFromClient);
    W(TotalUdpBytesToClient);
}

void xPP_AccountUsage::DeserializeMembers() {
    R(AuthId);
    R(StartTimestampMS);
    R(PeriodMS);

    R(TotalTcpBytesFromClient);
    R(TotalTcpBytesToClient);
    R(TotalUdpBytesFromClient);
    R(TotalUdpBytesToClient);
}
