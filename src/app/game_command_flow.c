#include "app/game_command_flow.h"
#include "app/property_flow.h"

#include <string.h>

#include "core/game_dispatch.h"
#include "core/game_constants.h"
#include "core/player_status.h"
#include "core/tool.h"

int rich4_app_dispatch_game_command(GameSession *session,
                                    const Rich4AppCommand *command,
                                    GameEventQueue *events)
{
    if (session == NULL || command == NULL ||
        command->type != RICH4_APP_COMMAND_GAME ||
        !rich4_app_command_is_valid(command)) {
        return 0;
    }
    return rich4_game_dispatch_command_deferred_confinement(
        &session->state, &session->flow, &command->game, events);
}

int rich4_app_apply_command(GameSession *session,
                            const Rich4AppCommand *app_command,
                            GameEventQueue *events)
{
    const GameCommand *command;

    if (events == NULL || session == NULL ||
        session->presentation.intro.active ||
        session->presentation.dice.popup_active ||
        app_command == NULL ||
        app_command->type != RICH4_APP_COMMAND_GAME ||
        !rich4_app_command_is_valid(app_command))
        return 0;

    command = &app_command->game;

    rich4_game_event_queue_reset(events);
    if (session->presentation.property.phase == GAME_PROPERTY_PRESENT_GOD_BUILD_CHOICE)
        return rich4_property_choose_god_building(session, command);

    if (command->type == GAME_COMMAND_USE_TOOL &&
        command->subject.kind == GAME_DOMAIN_TOOL &&
        command->subject.id == 9) {
        int current;
        int restored_player;
        uint32_t live_rng_state;
        PlayerStatusDecision decision;

        current = session->state.player_count > 0
            ? session->state.current_player % session->state.player_count : 0;
        if (current < 0) current += session->state.player_count;
        if (!session->flow.turn_flow_active &&
            rich4_player_status_evaluate(
                &session->state, current, 0, &decision) &&
            decision.status_slot >= 50)
            return 0;
        if (rich4_tool_inspect(&session->state, command) !=
                GAME_TOOL_USE_APPLIED || !session->time_machine_valid)
            return 0;
        restored_player = session->time_machine_state.player_count > 0
            ? session->time_machine_state.current_player %
                  session->time_machine_state.player_count : 0;
        if (restored_player < 0)
            restored_player += session->time_machine_state.player_count;
        if (session->time_machine_state.players[restored_player]
                .tool_inventory[9] == 0)
            return 0;
        /* 0x448544 restores map/player data, never the rand TLS word. */
        live_rng_state = session->state.ai_rng_state;
        session->state = session->time_machine_state;
        session->state.ai_rng_state = live_rng_state;
        session->flow = session->time_machine_flow;
        session->presentation = session->time_machine_presentation;
        session->state.players[restored_player].tool_inventory[9]--;
        /* A transfer snapshot may have been captured inside an AI tool
         * handler. Restoring it must not replay that pending command. */
        memset(&session->presentation.ai_tool, 0,
               sizeof(session->presentation.ai_tool));
        memset(session->presentation.tool_animation, 0,
               sizeof(session->presentation.tool_animation));
        session->presentation.tool_animation_head = 0;
        session->presentation.tool_animation_count = 0;
        memset(&session->presentation.tool_placement_animation, 0,
               sizeof(session->presentation.tool_placement_animation));
        session->presentation.tool_robot_tick = 0;
        memset(&session->presentation.tool_robot_motion, 0,
               sizeof(session->presentation.tool_robot_motion));
        session->presentation.monster_card.active = 0;
        session->presentation.monster_card.resource = 0;
        session->presentation.monster_card.sound_resource = 0;
        rich4_animation_clock_reset(&session->presentation.animation_clock[RICH4_ANIMATION_MONSTER]);
    session->presentation.monster_card.frame = 0;
        session->presentation.speech.active = 0;
        session->presentation.speech.current = NULL;
        rich4_character_speech_snapshot_clear(
            &session->presentation.speech.current_snapshot);
        (void)rich4_presentation_rebind_speech(&session->presentation);
        session->presentation.dice.cache_reset_pending = 1;
        session->flow.turn_flow_active = 0;
        session->flow.turn_state = GAME_TURN_STATE_IDLE;
        session->flow.steps_remaining = 0;
        session->presentation.dice.popup_active = 0;
        session->presentation.dice.status_visible = 0;
        session->presentation.actors[restored_player].dirty_flags |=
            GAME_PLAYER_DIRTY_TURN;
        return 1;
    }

    if (command->type == GAME_COMMAND_USE_TOOL &&
        command->subject.kind == GAME_DOMAIN_TOOL &&
        command->subject.id == 10 &&
        command->target.kind == GAME_DOMAIN_PLAYER &&
        command->target.id == session->state.current_player &&
        rich4_tool_inspect(&session->state, command) ==
            GAME_TOOL_USE_APPLIED) {
        session->time_machine_state = session->state;
        session->time_machine_flow = session->flow;
        session->time_machine_presentation = session->presentation;
        session->time_machine_valid = rich4_presentation_rebind_speech(
            &session->time_machine_presentation);
    }

    return rich4_app_dispatch_game_command(session, app_command, events);
}

int rich4_app_apply_game_command(GameSession *session,
                                 const GameCommand *command,
                                 GameEventQueue *events)
{
    Rich4AppCommand app_command;
    if (command == NULL) return 0;
    app_command = rich4_app_game_command(command);
    return rich4_app_apply_command(session, &app_command, events);
}

void rich4_app_sync_ai_card_animation(GameSession *session,
                                      const GameCommand *command,
                                      int accepted,
                                      int resource,
                                      int frame_count,
                                      uint32_t now)
{
    int speech_player = -1;

    if (!accepted || session == NULL || command == NULL ||
        command->source != GAME_COMMAND_SOURCE_AI ||
        command->type != GAME_COMMAND_USE_CARD ||
        command->subject.kind != GAME_DOMAIN_CARD || frame_count <= 0 ||
        session->presentation.monster_card.active)
        return;

    if (command->subject.id == 10 && resource == 557 &&
        command->target.kind == GAME_DOMAIN_LAND) {
        if (command->target.id >= 0 &&
            command->target.id < session->state.land_count)
            speech_player = session->state.land_owner[command->target.id];
    } else if (command->subject.id == 10 && resource == 557 &&
               command->target.kind == GAME_DOMAIN_FACILITY) {
        if (command->target.id >= 0 &&
            command->target.id < session->state.facility_count)
            speech_player = session->state.facility_owner[command->target.id];
    } else if (command->subject.id == 11 && resource == 529 &&
               (command->target.kind == GAME_DOMAIN_LAND ||
                command->target.kind == GAME_DOMAIN_FACILITY)) {
        /* The original card-12 handler uses Data[529] for the property
         * branch.  Dynamic object cleanup has no confirmed 0x45144F call, so
         * it deliberately remains state-only here. */
    } else {
        return;
    }

    if (speech_player < 0 || speech_player >= session->state.player_count)
        speech_player = -1;
    session->presentation.monster_card.active = 1;
    session->presentation.monster_card.resource = resource;
    session->presentation.monster_card.sound_resource = resource == 557 ?
        80 : 97;
    rich4_animation_clock_reset(&session->presentation.animation_clock[RICH4_ANIMATION_MONSTER]);
    session->presentation.monster_card.frame = 0;
    session->presentation.monster_card.speech_player =
        resource == 557 ? speech_player : -1;
    session->presentation.monster_card.started = now;
    session->presentation.monster_card.until =
        now + (uint32_t)(frame_count * 80 + 500);
}
