#include "platform/sdl1/game_lazy_assets.h"
#include <stdlib.h>

#include "app/dice_flow.h"
#include "app/app_command.h"
#include "app/command_event_flow.h"
#include "app/dynamic_object_flow.h"
#include "app/game_command_flow.h"
#include "app/game_save.h"
#include "app/game_session.h"
#include "app/new_game_setup_flow.h"
#include "app/tool_animation_flow.h"
#include "app/tool_followup_flow.h"
#include "app/insurance_flow.h"
#include "app/special_presentation_flow.h"
#include "core/game_command.h"
#include "core/game_constants.h"
#include "core/game_event_payload.h"
#include "core/property.h"
#include "core/tool.h"
#include "core/turn_flow.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/render/game_dice_render.h"
#include "platform/sdl1/render/game_scene_view.h"
#include "platform/sdl1/game_scene_view_builder.h"
#include "platform/sdl1/runtime_render_api.h"
#include "platform/sdl1/runtime.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/game_loop_turns.h"

static int game_apply_save_load_command(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, NewGameSettings *mutable_settings,
    const GameCommand *command)
{
    char path[RICH4_GAME_SAVE_PATH_CAPACITY];
    const MapData *map;
    int map_index;
    int slot;
    int accepted = 0;
    if (session == NULL || settings == NULL || command == NULL ||
        !session->state.active || !rich4_game_command_is_valid(command) ||
        command->source != GAME_COMMAND_SOURCE_HUMAN ||
        command->subject.kind != GAME_DOMAIN_SAVE_SLOT ||
        session->state.current_player < 0 ||
        session->state.current_player >= session->state.player_count ||
        command->player != session->state.current_player ||
        session->state.players[session->state.current_player].is_ai)
        return 0;
    slot = command->subject.id;
    if (!rich4_game_save_slot_path(path, sizeof(path), "saves", slot))
        return 0;
    if (command->type == GAME_COMMAND_SAVE_GAME) {
        if (!rich4_game_save_ensure_directory("saves")) return 0;
        return rich4_game_save_write(path, slot, session);
    }
    if (command->type == GAME_COMMAND_LOAD_GAME) {
        GameSession *loaded = (GameSession *)malloc(sizeof(*loaded));
        if (loaded == NULL) return 0;
        accepted = rich4_game_save_read(path, slot, loaded);
        map_index = accepted
            ? loaded->state.stage * GAME_MAPS_PER_STAGE + loaded->state.map
            : -1;
        map = assets != NULL && map_index >= 0 &&
            map_index < GAME_BACKGROUND_COUNT &&
            assets->map.maps[map_index].routes != NULL
                ? &assets->map.maps[map_index] : NULL;
        if (accepted &&
            !rich4_game_save_session_matches_map(loaded, map))
            accepted = 0;
        if (accepted && mutable_settings == NULL &&
            (loaded->state.stage != settings->stage ||
             loaded->state.map != settings->map))
            accepted = 0;
        if (accepted) {
            accepted = rich4_game_save_resume(session,loaded,rich4_sdl1_ticks());
            if (accepted && mutable_settings != NULL) {
                mutable_settings->stage = session->state.stage;
                mutable_settings->map = session->state.map;
            }
        }
        free(loaded);
        return accepted;
    }
    return 0;
}

int game_session_load_menu_slot(GameSession *session,
    const GamePreviewAssets *assets, NewGameSettings *settings, int slot)
{
    char path[RICH4_GAME_SAVE_PATH_CAPACITY];
    GameSession *loaded;
    int map_index;
    int accepted;
    if (session == NULL || assets == NULL || settings == NULL ||
        slot < GAME_SAVE_AUTO_SLOT || slot >= GAME_SAVE_SLOT_COUNT ||
        !rich4_game_save_slot_path(path, sizeof(path), "saves", slot))
        return 0;
    loaded = (GameSession *)malloc(sizeof(*loaded));
    if (loaded == NULL) return 0;
    accepted = rich4_game_save_read(path, slot, loaded);
    map_index = accepted
        ? loaded->state.stage * GAME_MAPS_PER_STAGE + loaded->state.map : -1;
    accepted = accepted && map_index >= 0 &&
        map_index < GAME_BACKGROUND_COUNT &&
        assets->map.maps[map_index].routes != NULL &&
        rich4_game_save_session_matches_map(
            loaded, &assets->map.maps[map_index]);
    if (accepted) {
        accepted = rich4_game_save_resume(session,loaded,rich4_sdl1_ticks());
    }
    if (accepted) {
        rich4_new_game_settings_from_session(settings, session);
    }
    free(loaded);
    return accepted;
}

static void sync_dynamic_card_presentation(GameSession *session,
                                           const GameCommand *command,
                                           int accepted, Uint32 now)
{
    if (!accepted || session == NULL || command == NULL ||
        command->type != GAME_COMMAND_USE_CARD ||
        command->subject.kind != GAME_DOMAIN_CARD)
        return;
    if (command->subject.id == 21) {
        /* 送神符 runs the original 0x40E32C release animation after the
         * core god state changes; its old map slot is cleared by the app
         * presentation helper. */
        if (!rich4_app_begin_dynamic_god_detach(
                &session->presentation, &session->state,
                command->player, (uint32_t)now))
            (void)rich4_app_detach_dynamic_god(
                &session->presentation, command->player);
    } else if (command->subject.id == 22 &&
               command->target.kind == GAME_DOMAIN_GOD) {
        /* 请神符 has already committed the same replacement attach in core;
         * release the old attached god before binding the new slot. */
        (void)rich4_app_begin_dynamic_god_detach(
            &session->presentation, &session->state,
            command->player, (uint32_t)now);
        (void)rich4_app_sync_dynamic_god_attachment(
            &session->presentation, &session->state,
            command->player, command->target.id);
    }
}

static void sync_ai_card_animation(
    GameSession *session, const GamePreviewAssets *assets,
    const GameCommand *command, int accepted, Uint32 now)
{
    const unsigned char *data = NULL;
    size_t size = 0;
    int resource = 0;
    int frames;

    if (!accepted || session == NULL || assets == NULL || command == NULL ||
        command->source != GAME_COMMAND_SOURCE_AI ||
        command->type != GAME_COMMAND_USE_CARD ||
        command->subject.kind != GAME_DOMAIN_CARD)
        return;
    if (command->subject.id == 10 &&
        command->target.kind == GAME_DOMAIN_LAND) {
        resource = 557;
        if (game_asset_ready(assets, &assets->ui.has_monster_card_animation)) {
            data = assets->ui.monster_card_data;
            size = assets->ui.monster_card_size;
        }
    } else if (command->subject.id == 10 &&
               command->target.kind == GAME_DOMAIN_FACILITY) {
        resource = 557;
        if (game_asset_ready(assets, &assets->ui.has_monster_card_animation)) {
            data = assets->ui.monster_card_data;
            size = assets->ui.monster_card_size;
        }
    } else if (command->subject.id == 11 &&
               (command->target.kind == GAME_DOMAIN_LAND ||
               command->target.kind == GAME_DOMAIN_FACILITY)) {
        resource = 529;
        if (game_asset_ready(assets, &assets->ui.has_demolish_card_animation)) {
            data = assets->ui.demolish_card_data;
            size = assets->ui.demolish_card_size;
        }
    } else {
        return;
    }
    frames = data != NULL ? game_graph_frame_count_data(data, size) : 0;
    if (frames <= 0) return;
    rich4_app_sync_ai_card_animation(
        session, command, accepted, resource, frames, (uint32_t)now);
}

/* R10.1d: command-to-event presentation bridge, moved verbatim out of
 * runtime_main.c. Core command dispatch remains in the domain layer; this
 * module only consumes its event queue and starts platform presentations. */
static int game_session_apply_command_internal(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, NewGameSettings *mutable_settings,
    const GameCommand *command, Uint32 now)
{
    GameEventQueue events;
    GameEvent event;
    Rich4AppCommand app_command;
    int accepted;

    if (command != NULL &&
        (command->type == GAME_COMMAND_SAVE_GAME ||
         command->type == GAME_COMMAND_LOAD_GAME)) {
        rich4_game_event_queue_reset(&events);
        return game_apply_save_load_command(
            session, assets, settings, mutable_settings, command);
    }
    app_command = rich4_app_game_command(command);
    accepted = rich4_app_apply_command(session, &app_command, &events);
    /* The acquisition bridge must see the old god's pending departure. */
    sync_dynamic_card_presentation(session, command, accepted, now);
    while (rich4_game_event_queue_pop(&events, &event)) {
        if (event.type == GAME_EVENT_GOD_ACQUIRED) {
            game_begin_god_presentation(session, assets, &event, now);
            continue;
        }
        {
            Rich4CommandEventResult app_result =
                rich4_app_apply_command_event(session, command, &event, now);
            if (app_result == RICH4_COMMAND_EVENT_REJECTED) {
                accepted = 0;
                continue;
            }
            if (app_result == RICH4_COMMAND_EVENT_CONSUMED)
                continue;
        }
        {
            Rich4CommandEventEffect effect =
                rich4_app_command_event_effect(session, command, &event);
            switch (effect.kind) {
            case RICH4_COMMAND_EVENT_EFFECT_STATUS_SYNC:
                game_sync_confinement_event(
                    session, assets, settings, &event);
                break;
            case RICH4_COMMAND_EVENT_EFFECT_PROPERTY_LEVEL:
                game_property_begin_level_presentation(session, assets,
                                                       &event, now);
                break;
            case RICH4_COMMAND_EVENT_EFFECT_TOOL_ANIMATION:
                game_tool_animation_enqueue_from_platform(
                    session, assets, &event, now);
                if (effect.mark_robot_trigger)
                    rich4_tool_followup_mark_robot_trigger(session, now);
                break;
            case RICH4_COMMAND_EVENT_EFFECT_NONE:
            default:
                /* Domain events such as money transfer, owner/level change and
                 * scene refresh describe a command that has already committed.
                 * They are not command failures. */
                break;
            }
        }
    }
    sync_ai_card_animation(session, assets, command, accepted, now);
    return accepted;
}

int game_session_apply_command(GameSession *session,
                               const GamePreviewAssets *assets,
                               const NewGameSettings *settings,
                               const GameCommand *command,
                               Uint32 now)
{
    return game_session_apply_command_internal(
        session, assets, settings, NULL, command, now);
}

int game_session_apply_command_with_map_switch(
    GameSession *session, const GamePreviewAssets *assets,
    NewGameSettings *settings, const GameCommand *command, Uint32 now)
{
    return game_session_apply_command_internal(
        session, assets, settings, settings, command, now);
}
