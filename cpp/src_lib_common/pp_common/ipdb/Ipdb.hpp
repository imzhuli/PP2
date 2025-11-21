#pragma once
#include "../_.hpp"
#include "../_region.hpp"

#include <netinet/in.h>

#include <mutex>

#define ZEC_USE_IP2LOC_DB

class xIpDb : xNonCopyable {
public:
    bool                Init(const char * RegionListFile, const char * DbFilename);
    void                Reopen();
    void                Clean();
    xOptional<xGeoInfo> Get(const char * IpStr);
    xOptional<xGeoInfo> Get(const in_addr_t IpAddr);

private:
    void *      DbPtr = nullptr;
    std::string DbFilename;
    std::mutex  DbMutex;

    std::filesystem::file_time_type DbFileTimestamp = {};
};
