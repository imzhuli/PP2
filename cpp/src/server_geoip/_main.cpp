#include "../../pb/pp2/geo.pb.h"
#include "../common/base.hpp"
#include "../common_protocol/protocol.hpp"
#include "../component/geo_ip.hpp"

#include <core/core_min.hpp>
#include <server_arch/client.hpp>

static auto HW = "Hello world!";
static auto GS = xGeoService{};

class xGeoIpConsumer : public xClient {
public:
	using xClient::Clean;
	using xClient::Init;

	bool Init(xIoContext * IoContextPtr, const xNetAddress & TargetAddress) {
		return xClient::Init(IoContextPtr, TargetAddress, xNetAddress::Parse("127.0.0.1"));
	}

	void OnServerConnected() override {
		X_DEBUG_PRINTF("");
		ubyte Buffer[MaxPacketSize];
		auto  RSize = xPacket::MakeRegisterDispatcherConsumer(Buffer, sizeof(Buffer), InterestedCommandIds.data(), InterestedCommandIds.size());
		PostData(Buffer, RSize);
	}

	bool OnPacket(const xPacketHeader & Header, ubyte * PayloadPtr, size_t PayloadSize) {
		switch (Header.CommandId) {
			case Cmd_GeoQuery:
				OnGeoQuery(Header, PayloadPtr, PayloadSize);
				break;
			default:
				break;
		}
		return true;
	}

	void OnGeoQuery(const xPacketHeader & Header, ubyte * PayloadPtr, size_t PayloadSize) {
		auto Req = geo::xGeoInfoReq();
		if (!Req.ParseFromArray(PayloadPtr, PayloadSize)) {
			return;
		}
		if (Req.hello_world() != HW) {
			return;
		}
		auto RA = xNetAddress::Parse(Req.ip());
		auto AS = RA.IpToString();
		auto GI = GS.GetRegionId(AS.c_str());
		X_DEBUG_PRINTF("GI: \n%s", HexShow(&GI.CountryId, sizeof(GI.CountryId)).c_str());

		auto Resp = geo::xGeoInfoResp();
		Resp.set_ip(AS);
		Resp.set_country_id(GI.CountryId);
		Resp.set_city_id(GI.CityId);
		ubyte  RespBuffer[MaxPacketSize];
		size_t RespSize = PbWritePacket(Cmd_GeoQueryResp, Header.RequestId, RespBuffer, sizeof(RespBuffer), Resp);

		PostData(RespBuffer, RespSize);
	}

	std::vector<xPacketCommandId> InterestedCommandIds = { 0x01 };
};

class xGeoIpUdpServer : public xUdpService {
protected:
	void OnPacket(const xNetAddress & RemoteAddress, const xPacketHeader & Header, ubyte * PayloadPtr, size_t PayloadSize) override {
		if (Header.CommandId != Cmd_GeoQuery) {
			return;
		}
		X_DEBUG_PRINTF("OnPacket: from:%s \n%s", RemoteAddress.ToString().c_str(), HexShow(PayloadPtr, PayloadSize).c_str());
		auto Req = geo::xGeoInfoReq();
		if (!Req.ParseFromArray(PayloadPtr, PayloadSize)) {
			return;
		}
		if (Req.hello_world() != HW) {
			return;
		}
		auto RA = xNetAddress::Parse(Req.ip());
		if (RA) {
			X_DEBUG_PRINTF("QueryByRequest: %s", RA.IpToString().c_str());
		} else {
			RA = RemoteAddress;
			X_DEBUG_PRINTF("QueryBySource: %s", RA.IpToString().c_str());
		}
		auto AS = RA.IpToString();
		auto GI = GS.GetRegionId(AS.c_str());
		X_DEBUG_PRINTF("GI: \n%s", HexShow(&GI.CountryId, sizeof(GI.CountryId)).c_str());

		auto Resp = geo::xGeoInfoResp();
		Resp.set_ip(AS);
		Resp.set_country_id(GI.CountryId);
		Resp.set_city_id(GI.CityId);
		ubyte  RespBuffer[MaxPacketSize];
		size_t RespSize = PbWritePacket(Cmd_GeoQueryResp, Header.RequestId, RespBuffer, sizeof(RespBuffer), Resp);

		X_DEBUG_PRINTF("PostData: %s\n%s", RemoteAddress.ToString().c_str(), HexShow(RespBuffer, RespSize).c_str());
		PostData(RespBuffer, RespSize, RemoteAddress);
	}
};

auto IoCtx     = xIoContext();
auto Consumer  = xGeoIpConsumer();
auto UdpServer = xGeoIpUdpServer();

int main(int argc, char ** argv) {

	auto CL = xCommandLine(
		argc, argv,
		{
			{ 'd', nullptr, "dispatcher_consumer", true },
			{ 'u', nullptr, "udp_bind_address", true },
			{ 'f', nullptr, "geo_filename", true },
		}
	);

	auto GF   = "./test_assets/gl2-city.mmdb";
	auto OptG = CL["geo_filename"];
	if (OptG()) {
		GF = OptG->c_str();
	}
	auto GG = xResourceGuard(GS, GF);
	assert(GG);

	auto OptD = CL["dispatcher_consumer"];
	if (!OptD()) {
		cerr << "invalid usage, need dispatcher address" << endl;
		return -1;
	}
	auto OptU = CL["udp_bind_address"];
	if (!OptD()) {
		cerr << "invalid usage" << endl;
		return -1;
	}

	auto BAddress  = xNetAddress::Parse(OptD->c_str());
	auto UBAddress = xNetAddress::Parse(OptU->c_str());
	auto IG        = xResourceGuard(IoCtx);
	auto OG        = xResourceGuard(Consumer, &IoCtx, BAddress);
	auto UG        = xResourceGuard(UdpServer, &IoCtx, UBAddress);

	auto T = xTicker();
	while (true) {
		T.Update();
		IoCtx.LoopOnce();
		Consumer.Tick(T);
	}
}
