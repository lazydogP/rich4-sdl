#include "core/rng.h"

#include <stdio.h>

int main(void)
{
    static const int expected[8] = {
        26601, 18277, 16324, 73, 20121, 8909, 19573, 21415
    };
    int first[8];
    Rich4Rng a, b;
    int i;
    rich4_rng_seed(311U);
    for (i = 0; i < 8; ++i) {
        first[i] = rich4_rng_next();
        if (first[i] != expected[i]) {
            fprintf(stderr, "fixed-width RNG golden differs at %d\n", i);
            return 1;
        }
    }
    rich4_rng_seed(311U);
    for (i = 0; i < 8; ++i) {
        if (first[i] != rich4_rng_next()) {
            fprintf(stderr, "fixed seed sequence differs at %d\n", i);
            return 1;
        }
    }
    if (rich4_rng_range(0) != 0) return 1;
    rich4_rng_state_seed(&a, 311U);
    rich4_rng_state_seed(&b, 311U);
    for (i = 0; i < 8; ++i) {
        if (rich4_rng_state_next(&a) != expected[i] ||
            rich4_rng_state_next(&b) != expected[i]) return 1;
    }
    if (a.state != b.state || rich4_rng_state_range(NULL, 10) != 0)
        return 1;
    rich4_rng_seed(77U);
    rich4_rng_global_export(&a);
    (void)rich4_rng_next();
    rich4_rng_global_import(&a);
    if (rich4_rng_next() != rich4_rng_state_next(&a)) return 1;
    /* Both explicit pointer and copied-state APIs consume the same word;
     * zero is a valid seed, never an implicit request to reseed with one. */
    a.state = 0;
    b.state = 0;
    if (rich4_rng_next_at(&a.state) != 0 || a.state != 12345u) return 1;
    (void)rich4_rng_original_state_next(&b);
    if (rich4_rng_range_at(&a.state, 6) !=
        rich4_rng_original_state_range(&b, 6) || a.state != b.state) return 1;
    puts("unit rng: ok");
    return 0;
}
