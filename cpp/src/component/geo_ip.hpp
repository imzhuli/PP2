#pragma once
#include "../common/base.hpp"

struct xRegionId {
	uint32_t CountryId = 0;
	uint32_t CityId    = 0;
};

class xGeoService {
public:
	bool Init(const char * IpDbFile);
	void Clean();

	xRegionId GetRegionId(const char * IpStr);

private:
	xVariable Native = {};
};
