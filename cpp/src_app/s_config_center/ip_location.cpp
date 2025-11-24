#include "ip_location.hpp"

static constexpr const int64_t IpDbReloadInterval = 10 * 60'000;

bool xCC_IpLocationManager::Init(const std::string & RegionMapFile, const std::string & DbName) {
    IpDbName = DbName;
    RuntimeAssert(IpDb.Init(RegionMapFile.c_str(), DbName.c_str()));

    ReloadIpDB();
    return true;
}

void xCC_IpLocationManager::Clean() {
    // TODO: cleanup ipdb
    IpDb.Clean();
}

void xCC_IpLocationManager::Tick(uint64_t NowMS) {
    auto Interval = SignedDiff(NowMS, LastUpdateDbTime);
    if (Interval < IpDbReloadInterval) {
        return;
    }
    LastUpdateDbTime = NowMS;
    ReloadIpDB();
}

void xCC_IpLocationManager::ReloadIpDB() {
    assert(IpDbName.size());
    IpDb.Reopen();
}

xGeoInfo xCC_IpLocationManager::GetRegionByIp(const char * IpString) {
    auto Opt = IpDb.Get(IpString);
    if (!Opt) {
        return xGeoInfo();
    }
    return *Opt;
}

xContinentId xCC_IpLocationManager::GetContinentIdByCountry(xCountryId CountryId) {
    return 0;
}
