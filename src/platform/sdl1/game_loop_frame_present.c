/* R10.1d: end-of-frame cursor composition and presentation moved verbatim
 * out of runtime_main.c. */

#include "platform/sdl1/game_loop_frame_present.h"
#include "platform/sdl1/runtime.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "platform/sdl1/runtime_trace.h"
#include "app/property_flow.h"

static void request_animation_wakeup(const VirtualCursorPresentationContext *c)
{
    const GamePresentationState *p = c->session != NULL
        ? &c->session->presentation : NULL;
    if (c->settings != NULL &&
        (c->app_state == RICH4_SCENE_NEW_GAME || c->app_state == RICH4_SCENE_INTRO))
        rich4_sdl1_wake_at(c->settings->last_anim_tick +
                          (c->settings->transition_active ? 50u : 100u));
    if (p == NULL || c->app_state != RICH4_SCENE_GAME) return;
    if (p->point_minigame.active && !p->point_minigame.automatic &&
        p->point_minigame.phase != RICH4_POINT_MINIGAME_RESULT) {
        if (p->point_minigame.phase != RICH4_POINT_MINIGAME_INTRO)
            rich4_sdl1_wake_at(p->point_minigame.next_tick);
    }
    for (int channel = 0; channel < RICH4_ANIMATION_COUNT; channel++) {
        const Rich4AnimationClock *clock = &p->animation_clock[channel];
        uint32_t deadline;
        if (rich4_animation_clock_deadline(clock,
                rich4_sdl1_frame_start_ticks(), &deadline))
            rich4_sdl1_wake_at(deadline);
    }
    if (p->god_detachment.active && p->god_detachment.frame_delay_ms > 0)
        rich4_sdl1_wake_at(p->god_detachment.frame_tick +
                          (uint32_t)p->god_detachment.frame_delay_ms);
}

void game_present_runtime_frame(VirtualCursorPresentationContext *cursor)
{
    unsigned int started;
    if (cursor == NULL || cursor->screen == NULL)
        return;
    request_animation_wakeup(cursor);
    started = runtime_profile_now();
    draw_virtual_cursor_presentation(cursor);
    runtime_profile_detail(PROFILE_PRESENT_CURSOR, started);
    rich4_sdl1_present(cursor->screen);
    rich4_property_flash_frame_presented(cursor->session, rich4_sdl1_ticks());
    request_animation_wakeup(cursor);
    started = runtime_profile_now();
    rich4_audio_present_visual_frame(cursor->audio);
    runtime_profile_detail(PROFILE_PRESENT_AUDIO, started);
    runtime_profile_frame_phase(3);
    rich4_sdl1_finish_frame();
    runtime_profile_frame_phase(4);
}
