#pragma once
#include <pp_common/_common.hpp>

void InitContextManager();
void CleanContextManager();
void ContextTicker(uint64_t NowMS);
