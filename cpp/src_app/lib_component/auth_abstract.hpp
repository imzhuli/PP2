#pragma once
#include "./pa_future.hpp"

#include <pp_common/_common.hpp>

class xAuthAbstract : xAbstract {
public:
    virtual bool Validate(const std::string_view Account, const std::string_view Pass, xPA_AuthFeature & Future) = 0;
};
