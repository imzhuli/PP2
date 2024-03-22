#include "./geo_ip.hpp"

#include <maxminddb.h>

static inline uint32_t CountryShortToId(const char * CSPtr) {
	if (!CSPtr[0] || !CSPtr[1]) {
		return 0;
	}
	return (static_cast<uint32_t>(static_cast<uint8_t>(CSPtr[0])) << 8) + static_cast<uint8_t>(CSPtr[1]);
}

bool xGeoService::Init(const char * IpDbFile) {
	auto MMDbPtr = new MMDB_s();
	auto check   = MMDB_open(IpDbFile, 0, MMDbPtr);
	if (check != MMDB_SUCCESS) {
		return false;
	}
	Native.P = MMDbPtr;
	return true;
}

void xGeoService::Clean() {
	assert(Native.P);
	auto MMDbPtr = (MMDB_s *)Steal(Native.P);
	MMDB_close(MMDbPtr);
	delete MMDbPtr;
}

xRegionId xGeoService::GetRegionId(const char * IpStr) {
	auto MMDbPtr   = (MMDB_s *)Native.P;
	auto GAIError  = (int)0;
	auto MMDBError = (int)0;
	auto Result    = MMDB_lookup_string(MMDbPtr, IpStr, &GAIError, &MMDBError);
	if (!Result.found_entry) {
		return {};
	}

	auto Data   = MMDB_entry_data_s();
	auto status = MMDB_get_value(&Result.entry, &Data, "country", "iso_code", nullptr);
	if (MMDB_SUCCESS != status || !Data.has_data || Data.type != MMDB_DATA_TYPE_UTF8_STRING || Data.data_size < 2) {
		return {};
	}
	auto CountryId = CountryShortToId(Data.utf8_string);
	X_DEBUG_PRINTF("Country: %c%c", Data.utf8_string[0], Data.utf8_string[1]);

	auto CityStatus = MMDB_get_value(&Result.entry, &Data, "cities", "0", nullptr);
	if (MMDB_SUCCESS != CityStatus || !Data.has_data) {
		X_DEBUG_PRINTF("No city name found!");
	} else {
		auto CityName = std::string(Data.utf8_string, Data.data_size);
		X_DEBUG_PRINTF("CityName: %s", CityName.c_str());
	}

	return xRegionId{ CountryId };
}
