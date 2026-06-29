#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "platform/sdl1/runtime.h"
#include "app/animation_clock.h"

/* The runtime object for this target renames just SDL's clock/sleep symbols.
 * Exercise the actual interactive scheduler, including wrap and late work. */
static uint32_t now, slept;
uint32_t qa_pacing_ticks(void) { return now; }
void qa_pacing_delay(uint32_t ms) { slept += ms; now += ms; }

int main(void)
{
    rich4_sdl1_set_deterministic_ticks(0);
    now=100; slept=0; rich4_sdl1_begin_frame();
    now+=18; rich4_sdl1_finish_frame();
    assert(slept==2 && now==120);
    now=200; slept=0; rich4_sdl1_begin_frame();
    now+=35; rich4_sdl1_finish_frame();
    assert(slept==0 && now==235);
    now=300; slept=0; rich4_sdl1_begin_frame();
    rich4_sdl1_wake_at(306); now+=4; rich4_sdl1_finish_frame();
    assert(slept==2 && now==306);
    /* A 24 ms animation's next frame may be closer than the input quantum. */
    now=340; slept=0; rich4_sdl1_begin_frame();
    rich4_sdl1_wake_at(348); now+=10; rich4_sdl1_finish_frame();
    assert(slept==0 && now==350);
    {
        Rich4AnimationClock clock = {0};
        uint32_t deadline;
        now=400; slept=0; rich4_sdl1_begin_frame();
        assert(!rich4_animation_clock_step(&clock, now, 10));
        now=415;
        assert(rich4_animation_clock_deadline(&clock,
            rich4_sdl1_frame_start_ticks(), &deadline) && deadline==410);
        rich4_sdl1_wake_at(deadline); rich4_sdl1_finish_frame();
        assert(slept==0); /* no extra20ms poll after an overdue FLC frame */
        now=420; slept=0; rich4_sdl1_begin_frame();
        assert(!rich4_animation_clock_deadline(&clock,
            rich4_sdl1_frame_start_ticks(), &deadline));
        now+=4; rich4_sdl1_finish_frame();
        assert(slept==16); /* an inactive FLC cannot force continuous redraw */
    }
    now=UINT32_MAX-9; slept=0; rich4_sdl1_begin_frame();
    now+=15; rich4_sdl1_finish_frame();
    assert(slept==5 && now==10);
    slept=0; rich4_sdl1_set_deterministic_ticks(1);
    rich4_sdl1_begin_frame(); rich4_sdl1_finish_frame();
    assert(slept==0 && rich4_sdl1_ticks()==16);
    puts("SDL frame pacing: remaining deadline, overrun, wrap, replay clock ok");
    return 0;
}
