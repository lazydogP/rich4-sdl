#include "platform/sdl1/game_lazy_assets.h"
#include <stddef.h>
#include <stdio.h>
#include "platform/sdl1/runtime_trace.h"

#include "app/config_types.h"
#include "app/game_save.h"
#include "app/dice_flow.h"
#include "app/game_session.h"
#include "app/game_update_flow.h"
#include "app/turn_rotation_flow.h"
#include "core/game_domain.h"
#include "app/new_game_setup_flow.h"
#include "core/game_constants.h"
#include "core/game_types.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_loop_orchestration.h"
#include "platform/sdl1/game_loop_turns.h"
#include "platform/sdl1/game_route_api.h"
#include "platform/sdl1/runtime_render_api.h"
#include "view/game_view_model.h"

typedef struct Rich4SdlPresentationUpdateContext {
    GameSession *session;
    const GamePreviewAssets *assets;
    const NewGameSettings *settings;
    const Rich4Config *config;
    Rich4Audio *audio;
    Uint32 now;
    int cursor_x;
} Rich4SdlPresentationUpdateContext;

typedef struct Rich4SdlTurnStartUpdateContext {
    GameSession *session;
    const GamePreviewAssets *assets;
    const NewGameSettings *settings;
    Uint32 now;
} Rich4SdlTurnStartUpdateContext;

typedef struct Rich4SdlRouteUpdateContext {
    GameSession *session;
    const GamePreviewAssets *assets;
    const NewGameSettings *settings;
    const Rich4Config *config;
    Rich4Audio *audio;
    Uint32 now;
} Rich4SdlRouteUpdateContext;

typedef struct Rich4SdlFirstLandingUpdateContext {
    GameSession *session;
    const GamePreviewAssets *assets;
    const NewGameSettings *settings;
    Uint32 now;
} Rich4SdlFirstLandingUpdateContext;

typedef struct Rich4SdlDiceUpdateContext {
    GameSession *session;
    const GamePreviewAssets *assets;
    const NewGameSettings *settings;
    const Rich4Config *config;
    Uint32 now;
} Rich4SdlDiceUpdateContext;

typedef struct Rich4SdlMovementUpdateContext {
    GameSession *session;
    const GamePreviewAssets *assets;
    const NewGameSettings *settings;
    const Rich4Config *config;
    Rich4Audio *audio;
    Uint32 now;
} Rich4SdlMovementUpdateContext;

static int update_presentation(void *opaque)
{
    unsigned int started = runtime_profile_now();
    int result;
    Rich4SdlPresentationUpdateContext *context = opaque;
    result = game_update_presentation_dispatch(
        context->session, context->assets, context->settings, context->config,
        context->audio, context->now, context->cursor_x);
    if (!result && context->session->pending_turn_events.count > 0) {
        int background = context->session->state.stage * GAME_MAPS_PER_STAGE +
            context->session->state.map;
        const MapData *map = context->assets != NULL && background >= 0 &&
            background < GAME_BACKGROUND_COUNT ? &context->assets->map.maps[background] : NULL;
        result = rich4_app_resume_turn_events(context->session, map, context->now);
    }
    runtime_profile_span("update", "presentation", started, -1, 0);
    return result;
}

static void apply_turn_start(
    void *opaque, const Rich4TurnStartRequest *request)
{
    unsigned int started = runtime_profile_now();
    Rich4SdlTurnStartUpdateContext *context = opaque;
    game_apply_turn_start_request(
        context->session, context->assets, context->settings, context->now,
        request);
    runtime_profile_span("update", "turn_start", started, -1, 0);
}

static int update_route_events(void *opaque)
{
    unsigned int started = runtime_profile_now();
    int result;
    Rich4SdlRouteUpdateContext *context = opaque;
    result = game_update_route_events(
        context->session, context->assets, context->settings, context->config,
        context->audio, context->now);
    runtime_profile_span("update", "route", started, -1, 0);
    return result;
}

static int update_first_landing(void *opaque)
{
    unsigned int started = runtime_profile_now();
    int result;
    Rich4SdlFirstLandingUpdateContext *context = opaque;
    result = game_update_first_landing(
        context->session, context->assets, context->settings, context->now);
    runtime_profile_span("update", "landing", started, -1, 0);
    return result;
}

static int update_dice(void *opaque)
{
    unsigned int started = runtime_profile_now();
    int result;
    Rich4SdlDiceUpdateContext *context = opaque;
    int background_index = context->settings->stage * GAME_MAPS_PER_STAGE +
                           context->settings->map;
    const MapData *map = background_index >= 0 &&
        background_index < GAME_BACKGROUND_COUNT
            ? &context->assets->map.maps[background_index] : NULL;
    GamePlayerState *player = &context->session->state.players[
        wrap_index(context->session->state.current_player,
                   context->session->state.player_count)];
    int action_frames = 0;
    int special_road_node = map != NULL && player->node > 0 &&
        (size_t)player->node < map->route_count &&
        (map->routes[player->node].flags & 0x80000000u) != 0;
    int graph_frames = game_dice_graph_frame_count(
        context->assets, context->session->presentation.dice.count);
    int graph_delay_ms = context->config != NULL &&
        context->config->game_speed == 0 ? 50 :
        context->config != NULL && context->config->game_speed == 2 ? 20 :
        30;

    if (context->assets != NULL && player->character >= 0 &&
        player->character < GAME_JUMP_CHARACTER_COUNT) {
        int part = rich4_game_actor_action_part(
            player, special_road_node);
        if (game_asset_ready(context->assets, &context->assets->character.has_map_player[player->character][part])) {
            int chunks = (int)context->assets->character.map_player_spr[
                player->character][part].num_chunks;
            action_frames = chunks >= 8 ? chunks / 8 : chunks;
        }
    }
    result = rich4_dice_update(
        context->session, map, action_frames,
        game_move_frame_ms_from_config(context->config), graph_frames,
        graph_delay_ms, context->now);
    runtime_profile_span("update", "dice", started, -1, 0);
    return result;
}

static int update_movement(void *opaque)
{
    unsigned int started = runtime_profile_now();
    Rich4SdlMovementUpdateContext *context = opaque;
    game_update_turn_movement(
        context->session, context->assets, context->settings, context->config,
        context->audio, context->now);
    runtime_profile_span("update", "movement", started, -1, 0);
    return 1;
}

void rich4_sdl_update_game_session_adapter(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, const Rich4Config *config,
    Rich4Audio *audio, Uint32 now, int cursor_x)
{
    Rich4SdlPresentationUpdateContext presentation_context;
    Rich4SdlTurnStartUpdateContext turn_start_context;
    Rich4SdlRouteUpdateContext route_context;
    Rich4SdlFirstLandingUpdateContext first_landing_context;
    Rich4SdlDiceUpdateContext dice_context;
    Rich4SdlMovementUpdateContext movement_context;
    Rich4AppGameUpdateServices services;

    unsigned int started;
    if (session == NULL) return;
    presentation_context.session = session;
    presentation_context.assets = assets;
    presentation_context.settings = settings;
    presentation_context.config = config;
    presentation_context.audio = audio;
    presentation_context.now = now;
    presentation_context.cursor_x = cursor_x;
    turn_start_context.session = session;
    turn_start_context.assets = assets;
    turn_start_context.settings = settings;
    turn_start_context.now = now;
    route_context.session = session;
    route_context.assets = assets;
    route_context.settings = settings;
    route_context.config = config;
    route_context.audio = audio;
    route_context.now = now;
    first_landing_context.session = session;
    first_landing_context.assets = assets;
    first_landing_context.settings = settings;
    first_landing_context.now = now;
    dice_context.session = session;
    dice_context.assets = assets;
    dice_context.settings = settings;
    dice_context.config = config;
    dice_context.now = now;
    movement_context.session = session;
    movement_context.assets = assets;
    movement_context.settings = settings;
    movement_context.config = config;
    movement_context.audio = audio;
    movement_context.now = now;
    services.presentation_context = &presentation_context;
    services.turn_start_context = &turn_start_context;
    services.route_context = &route_context;
    services.first_landing_context = &first_landing_context;
    services.dice_context = &dice_context;
    services.movement_context = &movement_context;
    services.update_presentation = update_presentation;
    services.apply_turn_start = apply_turn_start;
    services.update_route_events = update_route_events;
    services.update_first_landing = update_first_landing;
    services.update_dice = update_dice;
    services.update_movement = update_movement;
    started = runtime_profile_now();
    (void)rich4_app_game_update(session, now, &services);
    runtime_profile_span("update", "app.total", started, -1, 0);
    started = runtime_profile_now();
    if (rich4_presentation_take_auto_save_pending(
            &session->presentation)) {
        char path[RICH4_GAME_SAVE_PATH_CAPACITY];
        if (config != NULL && config->auto_save) {
            if (rich4_game_save_ensure_directory("saves") &&
                rich4_game_save_slot_path(
                    path, sizeof(path), "saves", GAME_SAVE_AUTO_SLOT) &&
                rich4_game_save_write(path, GAME_SAVE_AUTO_SLOT, session))
                {
                    runtime_profile_span("update", "autosave", started, -1, 0);
                    return;
                }
            runtime_profile_span("update", "autosave.failed", started, -1, 0);
            fprintf(stderr, "automatic save failed for slot 0\n");
        }
    }
}
