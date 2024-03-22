#include "../common/base.hpp"
#include "../component/geo_ip.hpp"

int main(int argc, char ** argv) {

	auto G  = xGeoService();
	auto GG = xResourceGuard(G, "./test_assets/gl2-city.mmdb");

	G.GetRegionId("124.156.132.165");

	return 0;
}
