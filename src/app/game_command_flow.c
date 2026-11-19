#include "app/game_command_flow.h"
#include "app/dynamic_object_flow.h"
#include "app/property_flow.h"
#include "view/game_view_model.h"

#include <string.h>

#include "core/game_dispatch.h"
#include "core/game_constants.h"
#include "core/player_status.h"
#include "core/tool.h"

int rich4_app_tool_target_valid(const GameSession *session,
                                const GameCommand *command)
{
    if (session == NULL || command == NULL ||
        rich4_tool_inspect(&session->state, command) !=
            GAME_TOOL_USE_APPLIED)
        return 0;
    if (command->subject.kind == GAME_DOMAIN_TOOL &&
        command->subject.id >= 1 && command->subject.id <= 3 &&
        command->target.kind == GAME_DOMAIN_ROUTE_NODE &&
        rich4_app_dynamic_object_occupies_route_node(
            &session->presentation, command->target.id))
        return 0;
    return 1;
}

static void capture_missile_scene(GameSession *session, GameDomainRef target)
{
    const GameState *state = &session->state;
    GameMissileScene *scene = &session->missile_scene;
    rich4_tool_target_xy(state, target, &scene->world_x, &scene->world_y);
    for (int i = 0; i < state->land_count; ++i)
        scene->lands[i] = (GameMissileBuilding){state->land_owner[i], state->land_type[i], state->land_level[i]};
    for (int i = 0; i < state->facility_count; ++i)
        scene->facilities[i] = (GameMissileBuilding){state->facility_owner[i], state->facility_type[i], state->facility_level[i]};
    memcpy(scene->players, state->players, sizeof(scene->players));
    memcpy(scene->special_node, state->special_player_node, sizeof(scene->special_node));
    memcpy(scene->special_play_state, state->special_player_play_state, sizeof(scene->special_play_state));
    memcpy(scene->objects, session->presentation.dynamic_objects, sizeof(scene->objects));
}

int rich4_app_dispatch_game_command(GameSession *session,
                                    const Rich4AppCommand *command,
                                    GameEventQueue *events)
{
    GameCommand resolved_command;
    if (session == NULL || command == NULL ||
        command->type != RICH4_APP_COMMAND_GAME ||
        !rich4_app_command_is_valid(command)) {
        return 0;
    }
    resolved_command = command->game;
    if (resolved_command.type == GAME_COMMAND_USE_TOOL &&
        ((resolved_command.subject.id == 7 && resolved_command.source == GAME_COMMAND_SOURCE_HUMAN) ||
         (resolved_command.subject.id == 10 && resolved_command.target.kind == GAME_DOMAIN_PLAYER &&
          resolved_command.target.id == resolved_command.player))) {
        /* Only an accepted roll/transfer replaces this player's rewind point.
         * 0x40DD1F captures the old inventory, dice value and map. */
        GameState next = session->state;
        if (!rich4_game_dispatch_command_deferred_confinement(
                &next, &session->flow, &resolved_command, events)) return 0;
        int captured = rich4_app_time_machine_capture(session, resolved_command.player);
        session->state = next;
        if (resolved_command.subject.id == 7)
            session->presentation.dice.remote_snapshot_ready = captured;
        return 1;
    }
    if (resolved_command.type == GAME_COMMAND_USE_TOOL &&
        resolved_command.subject.kind == GAME_DOMAIN_TOOL &&
        resolved_command.subject.id >= 1 &&
        resolved_command.subject.id <= 3 &&
        !rich4_app_tool_target_valid(session, &resolved_command))
        return 0;
    if (resolved_command.type == GAME_COMMAND_USE_TOOL &&
        resolved_command.subject.kind == GAME_DOMAIN_TOOL &&
        (resolved_command.subject.id == 6 || resolved_command.subject.id == 12)) {
        capture_missile_scene(session, resolved_command.target);
        resolved_command = rich4_game_command_with_missile_direction(
            resolved_command, session->presentation.map_direction & 7);
        int accepted = rich4_game_dispatch_command_deferred_confinement(
            &session->state, &session->flow, &resolved_command, events);
        session->missile_scene_active = accepted;
        return accepted;
    }
    if (resolved_command.type == GAME_COMMAND_USE_TOOL && resolved_command.subject.id == 10 &&
        resolved_command.target.kind == GAME_DOMAIN_LOOSE_MAP_OBJECT) {
        GameMapDynamicObjectState *object = &session->presentation.dynamic_objects[resolved_command.target.id];
        int node = resolved_command.target2.id;
        if (!object->node || node <= 0 || node >= session->state.tool_route_node_count ||
            rich4_app_dynamic_object_occupies_route_node(&session->presentation, node) ||
            !rich4_game_dispatch_command_deferred_confinement(&session->state, &session->flow,
                                                              &resolved_command, events))
            return 0;
        object->node = (uint16_t)node;
        object->world_x = session->state.tool_route_x[node];
        object->world_y = session->state.tool_route_y[node];
        for (int i = 0; i < 4; ++i) {
            int neighbor = session->state.tool_route_next[node][i];
            if (neighbor <= 0 || neighbor >= session->state.tool_route_node_count) continue;
            object->node_direction = rich4_game_actor_direction_from_delta(
                session->state.tool_route_x[node] - session->state.tool_route_x[neighbor],
                session->state.tool_route_y[node] - session->state.tool_route_y[neighbor]);
            break;
        }
        object->flight_direction = object->node_direction;
        return 1;
    }
    return rich4_game_dispatch_command_deferred_confinement(
        &session->state, &session->flow, &resolved_command, events);
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
        PlayerStatusDecision decision;
        const GameState *snapshot_state;
        const GameFlowState *snapshot_flow;
        const GameActorPresentationState *snapshot_actors;
        const GameMapDynamicObjectState *snapshot_dynamic_objects;

        current = session->state.player_count > 0
            ? session->state.current_player % session->state.player_count : 0;
        if (current < 0) current += session->state.player_count;
        if (!session->flow.turn_flow_active &&
            rich4_player_status_evaluate(
                &session->state, current, 0, &decision) &&
            decision.status_slot >= 50)
            return 0;
        if (rich4_tool_inspect(&session->state, command) !=
                GAME_TOOL_USE_APPLIED ||
            !rich4_app_time_machine_get(session, current, &snapshot_state,
                                        &snapshot_flow,
                                        &snapshot_actors,
                                        &snapshot_dynamic_objects))
            return 0;
        restored_player = snapshot_state->player_count > 0
            ? snapshot_state->current_player % snapshot_state->player_count : 0;
        if (restored_player < 0)
            restored_player += snapshot_state->player_count;
        /* 0x448544 restores map/player data, never the rand TLS word. */
        if (!rich4_app_time_machine_restore(session, current)) return 0;
        if (session->state.players[restored_player].tool_inventory[9] > 0)
            session->state.players[restored_player].tool_inventory[9]--;
        /* A transfer snapshot may have been captured inside an AI tool
         * handler. Restoring it must not replay that pending command. */
        memset(&session->presentation.ai_tool, 0,
               sizeof(session->presentation.ai_tool));
        memset(&session->presentation.card_use, 0,
               sizeof(session->presentation.card_use));
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

void rich4_app_sync_card_animation(GameSession *session,
                                      const GameCommand *command,
                                      int accepted,
                                      int resource,
                                      int frame_count,
                                      int frame_delay_ms,
                                      uint32_t now)
{
    int speech_player = -1;

    if (!accepted || session == NULL || command == NULL ||
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
        resource == 557 && session->presentation.card_use.phase == GAME_CARD_USE_NONE
            ? speech_player : -1;
    session->presentation.monster_card.started = now;
    session->presentation.monster_card.until =
        now + (uint32_t)(frame_count * (frame_delay_ms > 0 ? frame_delay_ms : 1) + 500);
}
