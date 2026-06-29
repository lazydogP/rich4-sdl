#include "core/rng.h"

#include <stddef.h>
#include <stdint.h>

/* 0x456F23/0x456F2D: one rand word in the main thread's runtime data.
 * The process word serves setup/menu APIs; an active game passes its saved
 * word explicitly, so transaction copies never consume an unrelated stream. */
static uint32_t rng_state = 1;
#ifdef R4DIFF_TEST
static Rich4RngObserver rng_observer;
void rich4_rng_set_observer(Rich4RngObserver observer){rng_observer=observer;}
#define OBSERVE(k,a,b,r) do { if(rng_observer)rng_observer(k,a,b,r); } while(0)
#else
#define OBSERVE(k,a,b,r) ((void)0)
#endif

int rich4_rng_original_state_next(Rich4Rng *rng)
{
    uint32_t before;
    if (rng == NULL) return 0;
    before = rng->state;
    rng->state = rng->state * UINT32_C(1103515245) + UINT32_C(12345);
    OBSERVE(0,before,rng->state,(rng->state>>16)&0x7fffu);
    (void)before;
    return (int)((rng->state >> 16) & UINT32_C(0x7fff));
}

int rich4_rng_original_state_range(Rich4Rng *rng, int upper_bound)
{
    if (rng == NULL || upper_bound <= 0) return 0;
    return rich4_rng_original_state_next(rng) % upper_bound;
}

void rich4_rng_state_seed(Rich4Rng *rng, unsigned int seed)
{
    if (rng != NULL) OBSERVE(1,rng->state,(uint32_t)seed,seed);
    if (rng != NULL) rng->state = (uint32_t)seed;
}

int rich4_rng_state_next(Rich4Rng *rng)
{
    if (rng == NULL) return 0;
    return rich4_rng_original_state_next(rng);
}

int rich4_rng_state_range(Rich4Rng *rng, int upper_bound)
{
    if (rng == NULL || upper_bound <= 0) return 0;
    return rich4_rng_state_next(rng) % upper_bound;
}

int rich4_rng_next_at(uint32_t *state)
{
    Rich4Rng rng;
    int value;
    if (state == NULL) return 0;
    rng.state = *state;
    value = rich4_rng_state_next(&rng);
    *state = rng.state;
    return value;
}

int rich4_rng_range_at(uint32_t *state, int upper_bound)
{
    if (state == NULL || upper_bound <= 0) return 0;
    return rich4_rng_next_at(state) % upper_bound;
}

void rich4_rng_global_export(Rich4Rng *out)
{
    if (out != NULL) out->state = rng_state;
}

void rich4_rng_global_import(const Rich4Rng *in)
{
    if (in != NULL) rng_state = in->state;
}

void rich4_rng_seed(unsigned int seed)
{
    OBSERVE(1,rng_state,(uint32_t)seed,seed);
    rng_state = (uint32_t)seed;
}

int rich4_rng_next(void)
{
    Rich4Rng rng;
    int value;
    rng.state = rng_state;
    value = rich4_rng_state_next(&rng);
    rng_state = rng.state;
    return value;
}

int rich4_rng_range(int upper_bound)
{
    if (upper_bound <= 0) {
        return 0;
    }
    return rich4_rng_next() % upper_bound;
}
