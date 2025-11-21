#include "./Ipdb.hpp"

#if defined(ZEC_USE_IP2LOC_DB)

#include "./IP2Location.h"

#include <map>
#include <mutex>
#include <rapidcsv.hpp>
#include <unordered_map>

static std::vector<xGeoInfo>                   KnownGeoInfoList;
static std::map<uint32_t, size_t>              CityIdToReginInfoMap;
static std::unordered_map<std::string, size_t> StateNameToRegionIdMap;

static std::string MakeCombinedStateName(const std::string & CountryName, const std::string & StateName) {
    return CountryName + ":" + StateName;
}

static auto InitOnceFlag = std::once_flag();
static void InitKnownGeoInfoList(const char * filename) {
    RuntimeAssert(KnownGeoInfoList.empty());
    RuntimeAssert(CityIdToReginInfoMap.empty());
    RuntimeAssert(StateNameToRegionIdMap.empty());
    RuntimeAssert(filename);

    rapidcsv::Document doc(filename);
    auto               RowCount = (unsigned)doc.GetRowCount();
    X_DEBUG_PRINTF("Rows:%u, Cols:%u", RowCount, (unsigned)doc.GetColumnCount());

    auto ACS  = doc.GetColumn<std::string>("COUNTRY_ALPHA2_CODE");
    auto RGCS = doc.GetColumn<std::string>("REGION_CODE");
    auto RGNS = doc.GetColumn<std::string>("REGION_NAME");
    for (unsigned I = 0; I < RowCount; ++I) {
        const auto & CountryName = ACS[I];
        auto         CountryId   = CountryCodeToCountryId(CountryName.c_str());
        if (!CountryId) {
            X_PERROR("Invalid CountryId: SourceName: %s", ACS[I].c_str());
            continue;
        }
        auto CombinedStateName = MakeCombinedStateName(CountryName, RGNS[I]);
        auto StateId           = xStateId(I);
        auto CityId            = xCityId(StateId);

        // X_DEBUG_PRINTF("%s: %u / %u", RGNS[I].c_str(), (unsigned)CountryId, (unsigned)LocalRegionId);

        KnownGeoInfoList.push_back({
            .CountryId = CountryId,
            .StateId   = StateId,
            .CityId    = CityId,
        });
        do {
            auto [_, Inserted] = CityIdToReginInfoMap.insert_or_assign(CityId, I);
            if (!Inserted) {
                X_DEBUG_PRINTF("Insert failed: %x %s / %s(%s)", (unsigned)StateId, ACS[I].c_str(), RGNS[I].c_str(), RGCS[I].c_str());
            }
        } while (false);
        do {
            auto [_, Inserted] = StateNameToRegionIdMap.insert_or_assign(CombinedStateName, I);
            if (!Inserted) {
                X_PERROR("duplicated region name: %s", CombinedStateName.c_str());
            }
        } while (false);
    }
    RuntimeAssert(RowCount == KnownGeoInfoList.size(), "RowCount must match KnownGeoInfoList size");
    RuntimeAssert(RowCount == CityIdToReginInfoMap.size(), "RowCount must match CityIdToReginInfoMap size");
    RuntimeAssert(RowCount >= StateNameToRegionIdMap.size(), "RowCount must larger than or equal to RegionNameToRegionIdMap size");
}

bool xIpDb::Init(const char * RegionListFile, const char * DbFilename) {
    std::call_once(InitOnceFlag, InitKnownGeoInfoList, RegionListFile);
    this->DbFilename = DbFilename;
    Reopen();
    return DbPtr;
}

void xIpDb::Reopen() {
    auto MutexGuard = std::lock_guard(DbMutex);
    auto ErrorCode  = std::error_code();
    auto T          = std::filesystem::last_write_time(DbFilename, ErrorCode);
    if (ErrorCode || T == DbFileTimestamp) {
        return;
    }
    auto NewIP2LocationObj = IP2Location_open(DbFilename.c_str());
    if (!NewIP2LocationObj) {
        return;
    }
    if (-1 == IP2Location_set_lookup_mode(NewIP2LocationObj, IP2LOCATION_CACHE_MEMORY)) {
        IP2Location_close(NewIP2LocationObj);
        return;
    }
    if (DbPtr) {
        IP2Location_close((IP2Location *)Steal(DbPtr));
    }
    DbPtr           = NewIP2LocationObj;
    DbFileTimestamp = T;
    return;
}

void xIpDb::Clean() {
    auto MutexGuard = std::lock_guard(DbMutex);
    if (auto IP2LocationObj = (IP2Location *)Steal(DbPtr)) {
        IP2Location_close(IP2LocationObj);
    }
}

xOptional<xGeoInfo> xIpDb::Get(const char * IpStr) {
    auto MutexGuard = std::lock_guard(DbMutex);
    if (!DbPtr) {
        return {};
    }

    auto RecordPtr = IP2Location_get_all((IP2Location *)DbPtr, (char *)IpStr);
    auto CleanUp   = xScopeGuard([=] { IP2Location_free_record(RecordPtr); });
    if (!RecordPtr || !RecordPtr->country_short) {
        X_DEBUG_PRINTF("Ip to geo info not found in ip2loc database");
        return {};
    }
    X_DEBUG_PRINTF("country: %s, region: %s", RecordPtr->country_short, RecordPtr->region);
    auto CombinedStateName = MakeCombinedStateName(RecordPtr->country_short, RecordPtr->region);
    auto Iter              = StateNameToRegionIdMap.find(CombinedStateName);
    if (Iter == StateNameToRegionIdMap.end()) {
        X_DEBUG_PRINTF("Geo info not found in local region info map");
        return {};
    }
    auto Index = Iter->second;
    assert(Index < KnownGeoInfoList.size());

    auto & GI = KnownGeoInfoList[Index];
    X_DEBUG_PRINTF("Geo address found: %s --> %s: %u/%u/%u", IpStr, CombinedStateName.c_str(), (unsigned)GI.CountryId, (unsigned)GI.StateId, (unsigned)GI.CityId);
    return GI;
}

xOptional<xGeoInfo> xIpDb::Get(const in_addr_t IpAddr) {
    auto MutexGuard = std::lock_guard(DbMutex);
    Fatal("Not implemented");
    return {};
}

#else
bool xIpDb::Init(const char * DbFilename) {
    return true;
}
void xIpDb::Reopen() {
}
void xIpDb::Clean() {
}
xOptional<xGeoInfo> xIpDb::Get(const char * IpStr) {
    return {};
}
xOptional<xGeoInfo> xIpDb::Get(const in_addr_t IpAddr) {
    return {};
}

#endif