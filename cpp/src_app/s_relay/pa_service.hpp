#pragma once
#include <pp_common/_common.hpp>

void InitPAService(const xNetAddress & BindAddress);
void CleanPAService();
void PAServiceTicker(uint64_t NowMS);
