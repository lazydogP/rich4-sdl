#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <SDL/SDL.h>

#include "app/ai_flow.h"
#include "app/game_speech_flow.h"
#include "core/tool.h"
#include "platform/sdl1/game_loop_turns.h"
#include "app/dynamic_object_flow.h"
#include "app/game_session.h"
#include "app/route_facts.h"
#include "core/ai.h"
#include "core/game_constants.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_route_api.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/game_view_model_input.h"
#include "platform/sdl1/render/game_map_objects.h"
#include "view/game_view_model.h"

typedef struct GameAiCommandAdapter {
    Rich4AiCommandSink sink;
    GameSession *session;
    const GamePreviewAssets *assets;
    const NewGameSettings *settings;
} GameAiCommandAdapter;

static int game_ai_map_land_name_groups(const MapData *map,
                                        int *out_groups,
                                        int capacity)
{
    int count;
    int i;
    if (out_groups == NULL || capacity <= 0) return 0;
    for (i = 0; i < capacity; i++) out_groups[i] = -1;
    if (map == NULL || map->lands == NULL || map->land_count <= 1) return 0;
    count = (int)map->land_count - 1;
    if (count > capacity) count = capacity;
    for (i = 0; i < count; i++) {
        const MapLandRecord *record = &map->lands[i + 1];
        int j;
        /* 0x42107F and 0x419744 compare C names beginning at record + 4.
         * If the fixed map-name field has no terminator, defer that record
         * rather than treating an adjacent field as part of its name. */
        if (memchr(record->raw + 4, '\0', 20) == NULL) continue;
        out_groups[i] = i;
        for (j = 0; j < i; j++) {
            const MapLandRecord *previous = &map->lands[j + 1];
            if (out_groups[j] < 0 ||
                memchr(previous->raw + 4, '\0', 20) == NULL)
                continue;
            if (strcmp((const char *)record->raw + 4,
                       (const char *)previous->raw + 4) == 0) {
                out_groups[i] = out_groups[j];
                break;
            }
        }
    }
    return count;
}

static int game_ai_submit_command(Rich4AiCommandSink *sink,
                                  const GameCommand *command,
                                  uint32_t now)
{
    GameAiCommandAdapter *adapter = (GameAiCommandAdapter *)sink;
    if (command->type == GAME_COMMAND_USE_TOOL) {
        if (rich4_tool_inspect(&adapter->session->state, command) !=
                GAME_TOOL_USE_APPLIED ||
            !rich4_ai_tool_begin(adapter->session, command, now)) return 0;
        return RICH4_AI_COMMAND_DEFERRED;
    }
    return game_session_apply_command(
        adapter->session, adapter->assets, adapter->settings, command, now);
}

/* SDL adapter: construct map facts and submit app-selected commands through
 * the normal command bridge.  Selection and phase progression live in app. */
void game_run_ai_preactions(GameSession *session,
                            const GamePreviewAssets *assets,
                            const NewGameSettings *settings,
                            int player, Uint32 now)
{
    GameDomainRef current_property;
    uint8_t player_on_map[GAME_MAX_PLAYERS];
    GameAiCommandAdapter adapter;
    GameAiMapTargetContext target_context;
    GameAiGlobalProjection global_projection;
    const GameAiGlobalProjection *global_projection_ptr = NULL;
    GameViewModelInput view_input;
    GameViewModel game_view;
    GameViewportModel viewport;
    GameVisibleMapObject visible_objects[GAME_MAP_RENDER_QUEUE_CAPACITY];
    GameAiGlobalObjectProjection ai_objects[GAME_MAP_RENDER_QUEUE_CAPACITY];
    uint16_t occupied_dynamic_nodes[16];
    int ai_land_name_groups[GAME_MAX_MAP_PROPERTIES];
    int ai_object_count = 0;
    int ai_land_name_group_count;
    int object_id, object_type, object_index, god_type, i;
    int background_index = settings != NULL
        ? settings->stage * GAME_MAPS_PER_STAGE + settings->map : 0;
    const MapData *map = assets != NULL && background_index >= 0 &&
        background_index < GAME_BACKGROUND_COUNT
        ? &assets->map.maps[background_index] : NULL;
    memset(player_on_map, 0, sizeof(player_on_map));
    memset(&adapter, 0, sizeof(adapter));
    memset(&global_projection, 0, sizeof(global_projection));
    /* 0x40E13C records ground-object slots in route +38. Tools are read
     * from core state; the first 16 slots belong to presentation objects. */
    for (i = 0; i < 16; i++) {
        const GameMapDynamicObjectState *object =
            &session->presentation.dynamic_objects[i];
        if (object->node != 0 && object->attached_player == 0)
            occupied_dynamic_nodes[
                global_projection.occupied_dynamic_node_count++] = object->node;
    }
    global_projection.occupied_dynamic_nodes = occupied_dynamic_nodes;
    ai_land_name_group_count = game_ai_map_land_name_groups(
        map, ai_land_name_groups, GAME_MAX_MAP_PROPERTIES);
    global_projection.land_name_groups = ai_land_name_groups;
    global_projection.land_name_group_count = ai_land_name_group_count;
    target_context.nearest_dynamic_god =
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
    target_context.nearest_dynamic_god_type = 0;
    if (map != NULL && map->routes != NULL && player >= 0 &&
        player < session->state.player_count &&
        session->state.players[player].node > 0 &&
        (size_t)session->state.players[player].node < map->route_count &&
        player == session->state.current_player) {
        rich4_sdl1_game_view_model_input_build(
            &session->flow, &session->presentation, &view_input);
        rich4_game_viewport_build(&session->state, &view_input, map,
                                  &viewport);
        if (rich4_game_view_model_build(&session->state, &view_input, map,
                                        &game_view)) {
            ai_object_count = game_map_collect_ai_objects(
                assets, background_index, &game_view,
                session->presentation.dynamic_objects, visible_objects,
                GAME_MAP_RENDER_QUEUE_CAPACITY);
            for (i = 0; i < ai_object_count; i++) {
                ai_objects[i].raw_id =
                    (uint16_t)visible_objects[i].object_id;
                ai_objects[i].screen_x = visible_objects[i].x;
                /* Original 0x409DE7 inserts word_48A856 - 40 into its
                 * 440x440 pixel buffer; the renderer queue uses window Y. */
                ai_objects[i].screen_y = visible_objects[i].y - 40;
            }
            global_projection.object_table_available = 1;
            global_projection.objects = ai_objects;
            global_projection.object_count = ai_object_count;
        }
        global_projection.center_world_x = viewport.center_world_x;
        global_projection.center_world_y = viewport.center_world_y;
        global_projection.direction = viewport.direction;
        for (i = 0; i < session->state.player_count &&
                    i < GAME_MAX_PLAYERS; i++) {
            const GamePlayerState *actor = &session->state.players[i];
            int world_x = 0;
            int world_y = 0;
            int have_position = 0;
            if (view_input.actors[i].move_active) {
                world_x = view_input.actors[i].visual_x;
                world_y = view_input.actors[i].visual_y;
                have_position = 1;
            } else {
                int special_index =
                    actor->confinement_actor_type == GAME_CONFINEMENT_HOSPITAL
                        ? 1 : actor->confinement_actor_type ==
                            GAME_CONFINEMENT_PRISON ? 2 : -1;
                if (special_index >= 0 && map->specials != NULL &&
                    (size_t)special_index < map->special_count) {
                    world_x = map->specials[special_index].x;
                    world_y = map->specials[special_index].y;
                    have_position = 1;
                } else if (actor->node > 0 &&
                           (size_t)actor->node < map->route_count) {
                    world_x = map->routes[actor->node].x;
                    world_y = map->routes[actor->node].y;
                    have_position = 1;
                }
            }
            if (!have_position) continue;
            global_projection.positioned_player_mask |= 1u << i;
            global_projection.player_world_x[i] = world_x;
            global_projection.player_world_y[i] = world_y;
            if (!game_view.actors[i].visible || world_x == 0)
                continue;
            global_projection.visible_player_mask |= 1u << i;
        }
        global_projection_ptr = &global_projection;
    }
    current_property =
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
    rich4_app_route_object_at(
        map, session->state.players[player].node,
        &object_id, &object_type, &object_index);
    (void)object_id;
    if (object_type == GAME_ROUTE_OBJECT_LAND)
        current_property =
            rich4_game_domain_ref(GAME_DOMAIN_LAND, object_index);
    else if (object_type == GAME_ROUTE_OBJECT_FACILITY)
        current_property =
            rich4_game_domain_ref(GAME_DOMAIN_FACILITY, object_index);
    for (i = 0; i < session->state.player_count; i++)
        player_on_map[i] = (uint8_t)(
            (session->state.players[i].play_state &
             GAME_PLAYER_PLAY_STATE_MASK) != 0 &&
            session->state.players[i].days_disappearing == 0 &&
            session->state.players[i].node >= 0);
    object_id = global_projection.object_table_available
        ? rich4_app_find_nearest_dynamic_god_with_projection(
              &session->presentation, &session->state, player,
              &global_projection, &god_type)
        : rich4_app_find_nearest_dynamic_god(
              &session->presentation, &session->state, player, &god_type);
    if (object_id >= 0) {
        target_context.nearest_dynamic_god =
            rich4_game_domain_ref(GAME_DOMAIN_GOD, object_id);
        target_context.nearest_dynamic_god_type = god_type;
    }
    adapter.sink.submit = game_ai_submit_command;
    adapter.session = session;
    adapter.assets = assets;
    adapter.settings = settings;
    rich4_ai_run_preactions_with_projection(
        session, current_property, player_on_map, &target_context,
        global_projection_ptr, player, now, &adapter.sink);
}

int game_update_ai_tool(GameSession *session, const GamePreviewAssets *assets,
                         const NewGameSettings *settings, Rich4Audio *audio,
                         Uint32 now)
{
    GameAiToolPresentationState *tool;
    GameCommand command;
    int accepted;
    if (session == NULL) return 0;
    tool = &session->presentation.ai_tool;
    if (tool->phase == GAME_AI_TOOL_NONE) return 0;
    if (tool->phase == GAME_AI_TOOL_EFFECTS) {
        /* Let the normal effect/speech dispatcher run; resume the AI's MOVE
         * phase only after the complete handler presentation has returned. */
        if (session->presentation.speech.active ||
            session->presentation.tool_animation_count ||
            session->presentation.tool_placement_animation.active ||
            session->presentation.tool_trigger_turn_pending ||
            session->state.tool_robot.active) return 0;
        if (rich4_ai_prepare_move(session, tool->command.player, &command))
            (void)game_session_apply_command(
                session, assets, settings, &command, now);
        command = rich4_game_command_roll_dice(
            GAME_COMMAND_SOURCE_AI, tool->command.player);
        tool->phase = GAME_AI_TOOL_NONE;
        if (!session->state.game_over)
            game_session_apply_command(session, assets, settings, &command, now);
        return 1;
    }
    if (session->presentation.speech.active &&
        game_update_property_speech(session, audio, now)) return 1;
    if (rich4_ai_tool_command_ready(session, now)) {
        command = tool->command;
        accepted = game_session_apply_command(
            session, assets, settings, &command, now);
        rich4_ai_tool_command_completed(session, accepted, now);
    }
    return 1;
}
