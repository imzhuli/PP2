#include "./static_ip_table.hpp"

#include <core/string.hpp>
#include <fstream>

xStaticIpTable LoadStaticIpTable(const char * Filename) {
	auto Infile = std::ifstream(Filename);
	if (!Infile) {
		return {};
	}

	auto Table = xStaticIpTable{};

	std::string line;
	while (std::getline(Infile, line)) {  // 逐行读取文件内容
		std::cout << line << std::endl;   // 打印每一行内容
		auto Segs = Split(line, "|");
		cout << Segs.size() << endl;
		if (Segs.size() != 3) {
			continue;
		}

		auto TerminalAddressString           = Trim(Segs[0]);
		auto TerminalControllerAddressString = Trim(Segs[1]);
		auto TerminalId                      = std::strtoll(Trim(Segs[2]).c_str(), nullptr, 0);

		auto Record       = xStaticIpRecord();
		Record.TerminalIp = xNetAddress::Parse(TerminalAddressString);
		RuntimeAssert(Record.TerminalIp && !Record.TerminalIp.Port);
		Record.TerminalControllerIp = xNetAddress::Parse(TerminalControllerAddressString);
		RuntimeAssert(Record.TerminalControllerIp && Record.TerminalControllerIp.Port);
		Record.TerminalId = TerminalId;
		RuntimeAssert(Record.TerminalId);

		Table.insert(std::make_pair(TerminalAddressString, std::move(Record)));
	}

	for (auto & P : Table) {
		cout << P.first << ": " << P.second.TerminalControllerIp.ToString() << ", " << P.second.TerminalId << endl;
	}
	return Table;
}
