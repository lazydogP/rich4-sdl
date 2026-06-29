/* R10.1d: confinement/hospital confirmation dispatch moved verbatim out of
 * runtime_main.c. */

#include "app/confinement_flow.h"
#include "platform/sdl1/game_loop_confirm_confinement.h"
#include "platform/sdl1/input_adapter.h"
#include "platform/sdl1/runtime.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/game_loop_audio_events.h"
#include "platform/sdl1/game_loop_panels.h"

int game_handle_confirm_confinement(
    Rich4SceneId app_state, GameSession *session,
    const GamePreviewAssets *assets, VirtualCursor *cursor,
    Rich4Audio *audio, int *host_pointer_sync_needed)
{
    Uint32 now;
    Rich4AudioEventQueue *audio_events;
    int handled = 0;
    if (app_state != RICH4_SCENE_GAME ||
        !session->presentation.confinement_visit.active)
        return 0;
    now = rich4_sdl1_ticks();
    audio_events = rich4_app_session_audio_borrow(session);
    if (session->presentation.confinement_visit.phase ==
        RICH4_CONFINEMENT_VISIT_SELECT) {
        int slot = cursor->confirm_pointer_pressed
            ? rich4_confinement_visit_slot_at(session, cursor->x, cursor->y)
            : session->presentation.confinement_visit.hover;
        if (slot >= 0) {
            handled = rich4_confinement_handle_select(
                session, slot, now, audio_events);
            if (handled && session->presentation.confinement_visit.phase ==
                    RICH4_CONFINEMENT_VISIT_CONFIRM) {
                int left, top, initial_x, initial_y;
                if (game_confirm_dialog_bounds(
                        assets, GAME_COMMAND_CONFIRM,
                        &left, &top, NULL, NULL) &&
                    rich4_sdl1_confirm_dialog_initial_cursor(
                        left, top, &initial_x, &initial_y)) {
                    cursor->x = initial_x;
                    cursor->y = initial_y;
                    *host_pointer_sync_needed = 1;
                }
            }
        }
    } else if (session->presentation.confinement_visit.phase ==
               RICH4_CONFINEMENT_VISIT_CONFIRM) {
        Rich4ConfirmDialogHit hit = RICH4_CONFIRM_DIALOG_ACCEPT;
        int left, top, width, height;
        if (cursor->confirm_pointer_pressed)
            hit = game_confirm_dialog_bounds(
                      assets, GAME_COMMAND_CONFIRM,
                      &left, &top, &width, &height)
                ? rich4_sdl1_confirm_dialog_hit(
                      cursor->x, cursor->y, left, top, width, height)
                : RICH4_CONFIRM_DIALOG_NONE;
        if (hit == RICH4_CONFIRM_DIALOG_DECLINE ||
            hit == RICH4_CONFIRM_DIALOG_ACCEPT) {
            handled = rich4_confinement_handle_confirm(
                session, hit == RICH4_CONFIRM_DIALOG_ACCEPT,
                now, audio_events);
        }
    } else if (session->presentation.confinement_visit.kind ==
               GAME_CONFINEMENT_HOSPITAL) {
        /* WM_LBUTTONUP outside state 2 dismisses the active #NNNN line
         * through sub_44EE18(1). */
        handled = 1;
        rich4_confinement_visit_skip_speech(session, now, audio_events);
    }
    game_session_audio_flush(session, audio);
    cursor->confirm_pressed = 0;
    (void)handled;
    return 1;
}
