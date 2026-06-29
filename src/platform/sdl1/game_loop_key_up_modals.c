/* R10.1d: SDL_KEYUP modal interruption hooks moved verbatim out of
 * runtime_main.c. */

#include <SDL/SDL.h>

#include "platform/sdl1/game_loop_key_up_modals.h"
#include "app/intro_flow.h"
#include "app/special_presentation_flow.h"
#include "app/special_place_notice_flow.h"
#include "app/god_money_flow.h"
#include "app/game_speech_flow.h"
#include "app/insurance_flow.h"
#include "app/property_flow.h"
#include "platform/sdl1/game_loop_turns.h"
#include "platform/sdl1/runtime.h"

/* 0x40F35E -> 0x40E2A2 -> 0x4528B9: death's main text wait
 * accepts key-up and left/right button-up after the FLC returns. */
static void god_release_input(GameSession *session)
{
    GameGodAcquisitionPresentationState *god = &session->presentation.god_acquisition;
    if (god->death_pending && !god->waiting_for_detach &&
        !god->arrival_speech_phase && god->message_visible == RICH4_GOD_MESSAGE_MAIN)
        god->until = rich4_sdl1_ticks();
    else
        rich4_god_money_stop(session);
}

void game_handle_key_up_modals(GameKeyUpModalsContext *context)
{
    if (context->app_state == RICH4_SCENE_GAME &&
        rich4_company_issue_skip_notice(context->session, rich4_sdl1_ticks())) return;
    if (context->app_state == RICH4_SCENE_GAME &&
        rich4_special_place_notice_skip(context->session, rich4_sdl1_ticks())) return;
    if (context->app_state == RICH4_SCENE_GAME &&
        rich4_road_reward_skip(context->session, rich4_sdl1_ticks())) return;
    if (context->app_state == RICH4_SCENE_GAME &&
        rich4_ai_tool_skip_notice(context->session)) return;
    if (context->app_state == RICH4_SCENE_GAME &&
        rich4_presentation_god_owns_frame(&context->session->presentation)) {
        god_release_input(context->session);
        return;
    }
    if (context->app_state == RICH4_SCENE_GAME &&
        context->session->presentation.speech.active) {
        /* Any WM_KEYUP, not only confirm/cancel, ends the wait. */
        rich4_speech_request_skip(context->session);
        return;
    }
    if (context->app_state == RICH4_SCENE_GAME &&
        rich4_insurance_flow_skip(context->session, rich4_sdl1_ticks())) return;
    if (context->app_state == RICH4_SCENE_GAME &&
        rich4_property_skip_wait(context->session, rich4_sdl1_ticks()))
        return;
    if (context->app_state == RICH4_SCENE_GAME &&
        context->session->presentation.special_event.active)
        game_skip_special_event_wait(
            context->session, context->audio, rich4_sdl1_ticks());
    if (context->app_state == RICH4_SCENE_GAME &&
        context->session->presentation.intro.active) {
        /* flags=3 enables interruption for any WM_KEYUP. */
        rich4_intro_request_skip(context->session);
    }
}

void game_handle_mouse_button_up_modals(
    Rich4SceneId app_state, GameSession *session, Rich4Audio *audio,
    int button)
{
    if (session == NULL ||
        (button != SDL_BUTTON_LEFT && button != SDL_BUTTON_RIGHT))
        return;
    if (app_state == RICH4_SCENE_GAME &&
        rich4_company_issue_skip_notice(session, rich4_sdl1_ticks())) return;
    if (app_state == RICH4_SCENE_GAME &&
        rich4_special_place_notice_skip(session, rich4_sdl1_ticks())) return;
    if (app_state == RICH4_SCENE_GAME &&
        rich4_road_reward_skip(session, rich4_sdl1_ticks())) return;
    if (app_state == RICH4_SCENE_GAME &&
        rich4_ai_tool_skip_notice(session)) return;
    if (app_state == RICH4_SCENE_GAME &&
        rich4_presentation_god_owns_frame(&session->presentation)) {
        god_release_input(session);
        return;
    }
    if (app_state == RICH4_SCENE_GAME &&
        session->presentation.speech.active) {
        /* sub_4544F6 treats WM_LBUTTONUP/WM_RBUTTONUP as an immediate
         * player_say interrupt. */
        rich4_speech_request_skip(session);
        return;
    }
    if (app_state == RICH4_SCENE_GAME &&
        rich4_insurance_flow_skip(session, rich4_sdl1_ticks())) return;
    if (app_state == RICH4_SCENE_GAME &&
        rich4_property_skip_wait(session, rich4_sdl1_ticks()))
        return;
    if (app_state == RICH4_SCENE_GAME &&
        session->presentation.special_event.active) {
        /* sub_4544F6, sub_45144F, and sub_4528B9 all consume button-up as
         * an interrupt in their respective waits. */
        game_skip_special_event_wait(session, audio, rich4_sdl1_ticks());
    }
    if (app_state == RICH4_SCENE_GAME &&
        session->presentation.intro.active) {
        /* sub_45144F flags bit 1 handles WM_LBUTTONUP and WM_RBUTTONUP,
         * then sub_415872 jumps to common cleanup. */
        rich4_intro_request_skip(session);
    }
}
