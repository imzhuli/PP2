#pragma once
#include "./base.hpp"

struct xRegionId {
	uint32_t CountryId;
	uint32_t CityId;
};

struct xRegionInfo {
	xRegionId   RegionId;
	std::string CountryName;
	std::string CityName;
};
