/* R10.1d: route-interaction confirmation dispatch moved verbatim out of
 * runtime_main.c. */

#include "platform/sdl1/game_loop_confirm_route_interaction.h"
#include "platform/sdl1/input_adapter.h"
#include "platform/sdl1/runtime.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_route_api.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/game_loop_input.h"
#include "platform/sdl1/game_loop_turns.h"
#include "app/bank_interaction_flow.h"
#include "app/route_flow.h"

int game_sync_route_interaction_initial_cursor(
    Rich4SceneId app_state, GameSession *session,
    const GamePreviewAssets *assets, VirtualCursor *cursor,
    int route_interaction_was_active)
{
    int left;
    int top;
    int initial_x;
    int initial_y;
    if (app_state != RICH4_SCENE_GAME ||
        route_interaction_was_active || session == NULL || cursor == NULL ||
        !session->presentation.route_interaction.active ||
        session->presentation.route_interaction.accept.source !=
            GAME_COMMAND_SOURCE_HUMAN ||
        session->presentation.route_interaction.accept.type ==
            GAME_COMMAND_BANK_WITHDRAW ||
        session->presentation.route_interaction.accept.type ==
            GAME_COMMAND_BANK_APPLY_LOAN)
        return 0;
    if (rich4_app_route_interaction_is_facility_choice(session)) {
        if (rich4_app_route_interaction_is_facility_research(session)) {
            cursor->x = 68;
            cursor->y = 324;
        } else {
            cursor->x = 84;
            cursor->y = 320;
        }
        rich4_app_route_set_hover(session, 0);
        return 1;
    }
    if (!game_confirm_dialog_bounds(
            assets, session->presentation.route_interaction.accept.type,
            &left, &top, NULL, NULL) ||
        !rich4_sdl1_confirm_dialog_initial_cursor(
            left, top, &initial_x, &initial_y))
        return 0;
    cursor->x = initial_x;
    cursor->y = initial_y;
    rich4_app_route_set_hover(
        session, RICH4_CONFIRM_DIALOG_ACCEPT);
    return 1;
}

int game_handle_confirm_route_interaction(
    Rich4SceneId app_state, GameSession *session,
    const GamePreviewAssets *assets, NewGameSettings *settings,
    VirtualCursor *cursor, Rich4Audio *audio, int mouse_left_down)
{
    int mandatory_facility_build;
    int facility_research;
    if (app_state != RICH4_SCENE_GAME ||
        !session->presentation.route_interaction.active)
        return 0;
    mandatory_facility_build =
        rich4_app_route_interaction_is_mandatory_facility_build(session);
    facility_research =
        rich4_app_route_interaction_is_facility_research(session);
    if (mandatory_facility_build || facility_research) {
        int choice = facility_research
            ? rich4_app_route_facility_research_choice_at(
                cursor->x, cursor->y)
            : rich4_app_route_facility_choice_at(cursor->x, cursor->y);
        int choice_count = mandatory_facility_build ? 5 :
            session->state.facility_level[
                session->presentation.route_interaction.accept.subject.id];
        if (cursor->confirm_pointer_pressed && mouse_left_down) {
            rich4_app_route_set_pressed(
                session, choice >= 0 && choice < choice_count ? choice : -1);
            cursor->confirm_pressed = 0;
            return 1;
        }
        if (!cursor->confirm_pointer_pressed &&
            session->presentation.route_interaction.hover >= 0)
            choice = session->presentation.route_interaction.hover;
        if (choice >= 0 && choice < choice_count) {
            GameCommand command = mandatory_facility_build
                ? rich4_app_route_facility_build_command(session, choice)
                : rich4_app_route_facility_research_command(session, choice);
            rich4_audio_play_ok(audio);
            game_session_apply_command(
                session, assets, settings, &command, rich4_sdl1_ticks());
        }
        rich4_app_route_set_pressed(session, -1);
    } else if (session->presentation.route_interaction.accept.type ==
                   GAME_COMMAND_BANK_WITHDRAW ||
               session->presentation.route_interaction.accept.type ==
                   GAME_COMMAND_BANK_APPLY_LOAN) {
        GameCommand command;
        int control = cursor->confirm_pointer_pressed
            ? game_bank_interaction_hit(
                  session, assets, cursor->x, cursor->y)
            : session->presentation.route_interaction.accept.type ==
                  GAME_COMMAND_BANK_WITHDRAW
                ? 17
                : rich4_app_bank_interaction_is_amount_entry(session)
                    ? 3
                    : rich4_app_bank_interaction_is_special_page(session)
                        ? RICH4_BANK_SPECIAL_BACK
                        : RICH4_BANK_LOAN_EXIT;
        if (rich4_app_bank_interaction_control_enabled(session, control)) {
            if (cursor->confirm_pointer_pressed && mouse_left_down)
                rich4_app_bank_interaction_set_pressed_control(session, control);
            if (session->presentation.route_interaction.accept.type ==
                    GAME_COMMAND_BANK_WITHDRAW)
                game_bank_play_press_sound(audio, control);
            else
                rich4_audio_play_ok(audio);
            if (game_bank_panel_apply_control(
                    session, control, cursor->x, &command))
                game_session_apply_command(
                    session, assets, settings, &command,
                    rich4_sdl1_ticks());
        }
    } else {
        Rich4ConfirmDialogHit hit = RICH4_CONFIRM_DIALOG_ACCEPT;
        int left, top, width, height;
        if (cursor->confirm_pointer_pressed) {
            hit = game_confirm_dialog_bounds(
                       assets,
                       session->presentation.route_interaction.accept.type,
                       &left, &top, &width, &height)
                ? rich4_sdl1_confirm_dialog_hit(
                      cursor->x, cursor->y, left, top, width, height)
                : RICH4_CONFIRM_DIALOG_NONE;
        }
        if (hit != RICH4_CONFIRM_DIALOG_NONE) {
            GameCommand command;
            Rich4AppRouteInteractionChoice choice =
                hit == RICH4_CONFIRM_DIALOG_DECLINE
                ? RICH4_APP_ROUTE_INTERACTION_DECLINE
                : RICH4_APP_ROUTE_INTERACTION_ACCEPT;
            if (!rich4_app_route_interaction_command_for_choice(
                    session, choice, &command))
                return 1;
            if (choice == RICH4_APP_ROUTE_INTERACTION_DECLINE)
                rich4_audio_play_cancel(audio);
            else
                rich4_audio_play_ok(audio);
            game_session_apply_command(
                session, assets, settings, &command, rich4_sdl1_ticks());
        }
    }
    cursor->confirm_pressed = 0;
    return 1;
}

int game_handle_cancel_route_interaction(
    Rich4SceneId app_state, GameSession *session,
    const GamePreviewAssets *assets, NewGameSettings *settings,
    VirtualCursor *cursor, Rich4Audio *audio)
{
    GameCommand command;
    int play_cancel_sound = 1;
    int apply_cancel = 1;
    int mandatory_facility_build;
    int facility_research;
    if (app_state != RICH4_SCENE_GAME ||
        session == NULL || !session->presentation.route_interaction.active)
        return 0;
    mandatory_facility_build =
        rich4_app_route_interaction_is_mandatory_facility_build(session);
    facility_research =
        rich4_app_route_interaction_is_facility_research(session);
    if (mandatory_facility_build) {
        /* 0x440AAC(0) ignores right-button cancellation. */
        apply_cancel = 0;
        cursor->cancel_pressed = 0;
        rich4_app_route_set_pressed(session, -1);
    } else if (facility_research) {
        command = session->presentation.route_interaction.decline;
        rich4_app_route_set_pressed(session, -1);
    } else if (session->presentation.route_interaction.accept.type ==
                   GAME_COMMAND_BANK_WITHDRAW ||
               session->presentation.route_interaction.accept.type ==
                   GAME_COMMAND_BANK_APPLY_LOAN) {
        if (!rich4_app_bank_interaction_cancel(
                session, rich4_sdl1_ticks(), &command)) {
            apply_cancel = 0;
            rich4_audio_play_cancel(audio);
        }
        /* 0x436EF8 closes the pass-bank panel on right-button-up without
         * calling sub_4542CE. */
        if (session->presentation.route_interaction.accept.type ==
                GAME_COMMAND_BANK_WITHDRAW)
            play_cancel_sound = 0;
    } else {
        if (!rich4_app_route_interaction_command_for_choice(
                session, RICH4_APP_ROUTE_INTERACTION_DECLINE, &command))
            apply_cancel = 0;
    }
    if (apply_cancel &&
        game_session_apply_command(
            session, assets, settings, &command, rich4_sdl1_ticks()) &&
        play_cancel_sound)
        rich4_audio_play_cancel(audio);
    cursor->cancel_pressed = 0;
    return 1;
}

int game_handle_route_interaction_mouse_button_up(
    Rich4SceneId app_state, GameSession *session,
    const GamePreviewAssets *assets, NewGameSettings *settings,
    VirtualCursor *cursor, Rich4Audio *audio, int button)
{
    if (app_state != RICH4_SCENE_GAME ||
        button != SDL_BUTTON_LEFT || session == NULL ||
        (!rich4_app_route_interaction_uses_confirm_dialog(session) &&
         !rich4_app_route_interaction_is_facility_choice(session)))
        return 0;
    if (rich4_app_route_interaction_is_facility_choice(session)) {
        int mandatory_facility_build =
            rich4_app_route_interaction_is_mandatory_facility_build(session);
        int choice = mandatory_facility_build
            ? rich4_app_route_facility_choice_at(cursor->x, cursor->y)
            : rich4_app_route_facility_research_choice_at(
                cursor->x, cursor->y);
        int choice_count = mandatory_facility_build ? 5 :
            session->state.facility_level[
                session->presentation.route_interaction.accept.subject.id];
        if (choice >= 0 && choice < choice_count) {
            GameCommand command = mandatory_facility_build
                ? rich4_app_route_facility_build_command(session, choice)
                : rich4_app_route_facility_research_command(session, choice);
            game_session_apply_command(
                session, assets, settings, &command, rich4_sdl1_ticks());
        }
        rich4_app_route_set_pressed(session, -1);
        cursor->confirm_pressed = 0;
        cursor->confirm_pointer_pressed = 0;
        return 1;
    }
    {
        int left, top, width, height;
        Rich4ConfirmDialogHit hit = RICH4_CONFIRM_DIALOG_NONE;
        if (game_confirm_dialog_bounds(
                assets, session->presentation.route_interaction.accept.type,
                &left, &top, &width, &height))
            hit = rich4_sdl1_confirm_dialog_activation(
                RICH4_APP_COMMAND_POINTER_UP,
                cursor->x, cursor->y, left, top, width, height);
        if (hit != RICH4_CONFIRM_DIALOG_NONE) {
            GameCommand command;
            Rich4AppRouteInteractionChoice choice =
                hit == RICH4_CONFIRM_DIALOG_DECLINE
                ? RICH4_APP_ROUTE_INTERACTION_DECLINE
                : RICH4_APP_ROUTE_INTERACTION_ACCEPT;
            if (!rich4_app_route_interaction_command_for_choice(
                    session, choice, &command))
                return 1;
            if (choice == RICH4_APP_ROUTE_INTERACTION_DECLINE)
                rich4_audio_play_cancel(audio);
            else
                rich4_audio_play_ok(audio);
            game_session_apply_command(
                session, assets, settings, &command, rich4_sdl1_ticks());
        }
    }
    cursor->confirm_pressed = 0;
    cursor->confirm_pointer_pressed = 0;
    return 1;
}
