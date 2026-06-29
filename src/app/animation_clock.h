#ifndef RICH4_APP_ANIMATION_CLOCK_H
#define RICH4_APP_ANIMATION_CLOCK_H

#include <stdint.h>

/* 0x45144F timestamps each decoded/displayed frame separately. A slow frame
 * consumes its own interval; it never creates a debt of frames to catch up.
 * The first update exposes frame zero, including after synchronous loading. */
typedef struct Rich4AnimationClock {
    uint32_t tick;
    uint32_t updated_at;
    int started;
    int delay_ms;
} Rich4AnimationClock;

enum Rich4AnimationChannel {
    RICH4_ANIMATION_INTRO, RICH4_ANIMATION_SPECIAL, RICH4_ANIMATION_TOOL,
    RICH4_ANIMATION_REWARD, RICH4_ANIMATION_BANKRUPTCY,
    RICH4_ANIMATION_GOD, RICH4_ANIMATION_DETACHMENT,
    RICH4_ANIMATION_MONSTER, RICH4_ANIMATION_MAX_LEVEL,
    RICH4_ANIMATION_DICE, RICH4_ANIMATION_MAGIC,
    RICH4_ANIMATION_MONTHLY, RICH4_ANIMATION_POINT_INTRO,
    RICH4_ANIMATION_ENDING, RICH4_ANIMATION_LANDING, RICH4_ANIMATION_PROPERTY_FLASH,
    RICH4_ANIMATION_COUNT
};

static inline void rich4_animation_clock_reset(Rich4AnimationClock *clock)
{
    clock->tick = 0;
    clock->updated_at = 0;
    clock->started = 0;
    clock->delay_ms = 0;
}

static inline int rich4_animation_clock_step(Rich4AnimationClock *clock,
                                             uint32_t now, int delay_ms)
{
    clock->updated_at = now;
    clock->delay_ms = delay_ms > 0 ? delay_ms : 1;
    if (!clock->started) {
        clock->started = 1;
        clock->tick = now;
        return 0;
    }
    if (delay_ms <= 0) delay_ms = 1;
    if ((int32_t)(now - clock->tick) < delay_ms) return 0;
    clock->tick = now;
    return 1;
}

/* An overdue active frame must wake immediately. An idle/finished clock's
 * historical deadline must not turn the main loop into a busy loop. */
static inline int rich4_animation_clock_deadline(const Rich4AnimationClock *clock,
    uint32_t frame_started, uint32_t *deadline)
{
    if (!clock->started || (int32_t)(clock->updated_at-frame_started) < 0)
        return 0;
    *deadline = clock->tick + (uint32_t)clock->delay_ms;
    return 1;
}

#endif
