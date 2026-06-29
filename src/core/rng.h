#ifndef RICH4_CORE_RNG_H
#define RICH4_CORE_RNG_H

#include <stdint.h>

typedef struct Rich4Rng {
    uint32_t state;
} Rich4Rng;
#ifdef R4DIFF_TEST
typedef void (*Rich4RngObserver)(unsigned kind,uint32_t before,uint32_t after,unsigned result);
void rich4_rng_set_observer(Rich4RngObserver observer);
#endif

void rich4_rng_state_seed(Rich4Rng *rng, unsigned int seed);
int rich4_rng_state_next(Rich4Rng *rng);
int rich4_rng_state_range(Rich4Rng *rng, int upper_bound);
/* Explicit access to the session's serialized shared rand state. */
int rich4_rng_next_at(uint32_t *state);
int rich4_rng_range_at(uint32_t *state, int upper_bound);
void rich4_rng_global_export(Rich4Rng *out);
void rich4_rng_global_import(const Rich4Rng *in);

void rich4_rng_seed(unsigned int seed);
int rich4_rng_next(void);
int rich4_rng_range(int upper_bound);

/* Rich4.exe 0x456F2D uses the Watcom/ANSI LCG, distinct from MSVCRT. */
int rich4_rng_original_state_next(Rich4Rng *rng);
int rich4_rng_original_state_range(Rich4Rng *rng, int upper_bound);
#endif
