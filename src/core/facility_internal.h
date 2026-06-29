#ifndef RICH4_CORE_FACILITY_INTERNAL_H
#define RICH4_CORE_FACILITY_INTERNAL_H

#include "core/facility.h"

extern const unsigned char rich4_facility_max_level[5];
int rich4_facility_bounded_value(int64_t value, int *out_value);
int rich4_facility_active_player(const GameState *state, int player);
int rich4_facility_valid_company(const GameState *state, int company);

#endif
