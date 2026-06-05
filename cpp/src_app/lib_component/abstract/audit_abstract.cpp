#include "./audit_abstract.hpp"

std::string xAuditUsage::ToString() const {
    auto OS = std::ostringstream();
    OS << "xAuditUsage" << endl;
    OS << "\tAuthId:" << AuthId << endl;
    OS << "\tStartTimestampMS:" << StartTimestampMS << endl;
    OS << "\tPeriodMS:" << PeriodMS << endl;
    OS << "\tTotalTcpBytesFromClient:" << TotalTcpBytesFromClient << endl;
    OS << "\tTotalTcpBytesToClient:" << TotalTcpBytesToClient << endl;
    OS << "\tTotalUdpBytesFromClient:" << TotalUdpBytesFromClient << endl;
    OS << "\tTotalUdpBytesToClient:" << TotalUdpBytesToClient << endl;
    return OS.str();
}
