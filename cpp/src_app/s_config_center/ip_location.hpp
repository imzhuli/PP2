#pragma once
#include <pp_common/_.hpp>
#include <pp_common/_region.hpp>
#include <pp_common/ipdb/Ipdb.hpp>

class xCC_IpLocationManager {
public:
    bool Init(const std::string & RegionMapFile, const std::string & DbName);
    void Clean();
    void Tick(uint64_t NowMS);
    void ReloadIpDB();

    xGeoInfo     GetRegionByIp(const char * IpString);
    xContinentId GetContinentIdByCountry(xCountryId CountryId);

private:
    std::string IpDbName;
    xIpDb       IpDb;
    uint64_t    LastUpdateDbTime = 0;
};
