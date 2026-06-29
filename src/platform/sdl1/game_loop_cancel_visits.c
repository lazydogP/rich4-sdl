/* R10.1d: shop/lottery/confinement cancel branches moved verbatim out of
 * runtime_main.c. */

#include "platform/sdl1/game_loop_cancel_visits.h"
#include "app/confinement_flow.h"
#include "app/shop_flow.h"
#include "platform/sdl1/input_adapter.h"
#include "platform/sdl1/runtime.h"
#include "platform/sdl1/game_loop_audio_events.h"
#include "platform/sdl1/game_loop_panels.h"

int game_handle_cancel_visits(GameCancelVisitsContext *context)
{
    if (context->app_state == RICH4_SCENE_GAME &&
        context->session->presentation.shop_visit.active) {
        /* Keyboard/controller cancel mirrors WM_RBUTTONUP. */
        if (context->session->presentation.shop_visit.phase !=
                RICH4_SHOP_OWNER_GIFT &&
            context->session->presentation.shop_visit.phase !=
                RICH4_SHOP_OWNER_SPEECH) {
            *context->shop_music_restore_pending =
                context->session->presentation.shop_visit.music_started;
            rich4_audio_stop_speech(context->audio);
            rich4_shop_finish(context->session);
        }
        context->cursor->cancel_pressed = 0;
        return 1;
    } else if (context->app_state == RICH4_SCENE_GAME &&
               context->session->presentation.lottery.active) {
        /* Only the explicit SDL right-button-up path mirrors
         * sub_42F7FC's WM_RBUTTONUP branch. */
        context->cursor->cancel_pressed = 0;
        return 1;
    } else if (context->app_state == RICH4_SCENE_GAME &&
               context->session->presentation.confinement_visit.active) {
        Uint32 now = rich4_sdl1_ticks();
        Rich4AudioEventQueue *audio_events =
            rich4_app_session_audio_borrow(context->session);
        (void)rich4_confinement_handle_cancel(
            context->session, now, audio_events);
        game_session_audio_flush(context->session, context->audio);
        context->cursor->cancel_pressed = 0;
        return 1;
    }
    return 0;
}
