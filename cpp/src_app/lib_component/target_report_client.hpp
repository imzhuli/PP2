#pragma once
#include <object/object.hpp>
#include <pp_common/_.hpp>
#include <random>

class xTargetReportClient final {
public:
    bool Init(const xNetAddress & ServerListServerAddress);
    void Clean();
};
