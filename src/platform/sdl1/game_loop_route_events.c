#include <stdint.h>

#include "app/insurance_flow.h"
#include "app/config_types.h"
#include "app/game_session.h"
#include "app/new_game_setup_flow.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "core/game_command.h"
#include "core/game_constants.h"
#include "core/game_event.h"
#include "core/game_event_payload.h"
#include "app/route_flow.h"
#include "app/route_event_flow.h"
#include "app/special_actor_flow.h"
#include "app/audio_event.h"
#include "core/turn_flow.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/input_adapter.h"
#include "platform/sdl1/render/game_scene_view.h"
#include "platform/sdl1/runtime_render_api.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_route_api.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/game_loop_panels.h"
#include "platform/sdl1/game_loop_turns.h"
#include "view/view_constants.h"
#include "resource/spr_smp.h"

typedef struct Rich4SdlRoutePropertyContext {
    GameSession *session;
    const GamePreviewAssets *assets;
    Rich4Audio *audio;
    Uint32 now;
} Rich4SdlRoutePropertyContext;

typedef struct Rich4SdlRoutePresentationContext {
    GameSession *session;
    const GamePreviewAssets *assets;
    const MapData *map;
    const NewGameSettings *settings;
    const Rich4Config *config;
    Rich4Audio *audio;
    Uint32 now;
} Rich4SdlRoutePresentationContext;

static int update_property_charge(void *opaque)
{
    Rich4SdlRoutePropertyContext *context = opaque;
    return game_property_update_charge_presentation(
        context->session, context->audio, context->now);
}

static void start_property_max_level(void *opaque)
{
    Rich4SdlRoutePropertyContext *context = opaque;
    game_property_start_max_level_animation(
        context->session, context->assets, context->now);
}

static int update_property_max_level(void *opaque)
{
    Rich4SdlRoutePropertyContext *context = opaque;
    return game_property_update_max_level_animation(
        context->session, context->assets, context->now);
}

static void present_route_event(
    void *opaque, const Rich4AppRoutePresentation *presentation)
{
    Rich4SdlRoutePresentationContext *context = opaque;
    if (presentation == NULL) return;
    switch (presentation->kind) {
    case RICH4_ROUTE_PRESENTATION_SPECIAL_EVENT:
        game_begin_special_event_presentation(
            context->session, context->assets, &presentation->event, context->audio,
            context->now);
        break;
    case RICH4_ROUTE_PRESENTATION_GOD_ANIMATION:
        game_tool_animation_enqueue_from_platform(context->session, context->assets,
                                                  &presentation->event, context->now);
        break;
    case RICH4_ROUTE_PRESENTATION_PROPERTY_LEVEL:
        game_property_begin_level_presentation(
            context->session, context->assets, &presentation->event,
            context->now);
        break;
    case RICH4_ROUTE_PRESENTATION_STATUS:
        game_sync_confinement_event(
            context->session, context->assets, context->settings,
            &presentation->event);
        break;
    case RICH4_ROUTE_PRESENTATION_SPECIAL_PLACE:
        game_begin_special_reward_presentation(
            context->session, context->assets, &presentation->event,
            context->audio, context->now);
        break;
    case RICH4_ROUTE_PRESENTATION_LOTTERY_VISIT:
        game_begin_lottery_visit(
            context->session, context->assets, &presentation->event, context->audio,
            context->now, context->config != NULL &&
                context->config->animation != 0);
        break;
    case RICH4_ROUTE_PRESENTATION_NONE:
    default:
        break;
    }
}

static void begin_special_actor(
    void *opaque, const Rich4AppTurnRotationResult *result)
{
    Rich4SdlRoutePresentationContext *context = opaque;
    if (result == NULL || !result->special_action_pending) return;
    rich4_special_actor_begin(
        context->session, context->map, &result->special_action);
}

static void consume_audio_event(void *opaque, const Rich4AudioEvent *event)
{
    Rich4SdlRoutePresentationContext *context = opaque;
    rich4_audio_consume_event(context->audio, event);
}

int game_update_route_events(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, const Rich4Config *config,
    Rich4Audio *audio, Uint32 now)
{
    int background_index;
    const MapData *map = NULL;
    Rich4SdlRoutePropertyContext property_context;
    Rich4SdlRoutePresentationContext presentation_context;
    Rich4AppRouteEventServices services;

    if (session == NULL) return 0;
    background_index = settings != NULL
        ? settings->stage * GAME_MAPS_PER_STAGE + settings->map : -1;
    if (assets != NULL && background_index >= 0 &&
        background_index < GAME_BACKGROUND_COUNT)
        map = &assets->map.maps[background_index];
    property_context.session = session;
    property_context.assets = assets;
    property_context.audio = audio;
    property_context.now = now;
    presentation_context.session = session;
    presentation_context.assets = assets;
    presentation_context.map = map;
    presentation_context.settings = settings;
    presentation_context.config = config;
    presentation_context.audio = audio;
    presentation_context.now = now;
    services.property_context = &property_context;
    services.presentation_context = &presentation_context;
    services.actor_context = &presentation_context;
    services.audio_context = &presentation_context;
    services.property_end_delay_ms =
        (uint32_t)game_move_frame_ms_from_config(config);
    services.update_property_charge = update_property_charge;
    services.update_property_max_level = update_property_max_level;
    services.start_property_max_level = start_property_max_level;
    services.present = present_route_event;
    services.begin_special_actor = begin_special_actor;
    services.consume_audio_event = consume_audio_event;
    return rich4_app_route_event_update(
        session, map, now, config != NULL && config->animation != 0,
        &services);
}
