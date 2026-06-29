#include "platform/sdl1/game_lazy_assets.h"
#include <SDL/SDL.h>
#include "platform/sdl1/runtime.h"

#include "app/config_types.h"
#include "app/game_session.h"
#include "app/special_presentation_flow.h"
#include "app/new_game_setup_flow.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "core/game_constants.h"
#include "core/game_event_payload.h"
#include "core/special_event.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/game_loop_audio_events.h"
#include "platform/sdl1/game_special_event_internal.h"
#include "platform/sdl1/game_view_model_input.h"
#include "platform/sdl1/render/game_map_objects.h"

static void game_special_event_graph_input(
    const unsigned char *data, size_t size, int *out_frames, int *out_delay)
{
    if (out_frames == NULL || out_delay == NULL) return;
    *out_frames = game_graph_frame_count_data(data, size);
    *out_delay = game_graph_frame_delay_ms_data(data, size);
}

static void game_prepare_special_event_update_input(
    const GameSession *session, const GamePreviewAssets *assets,
    Rich4SpecialEventUpdateInput *input, Uint32 now, int speech_playing)
{
    int asset_index;
    int consequence_index;
    const unsigned char *data;
    size_t size;

    *input = (Rich4SpecialEventUpdateInput){0};
    if (session == NULL) return;
    asset_index = session->presentation.special_event.phase == RICH4_SPECIAL_PRESENT_PAGE
        ? game_special_event_asset_index(assets, session->presentation.special_event.resource)
        : -1;
    if (asset_index >= 0) {
        game_special_event_graph_input(
            assets->ui.special_event_data[asset_index],
            assets->ui.special_event_size[asset_index],
            &input->page_frame_count, &input->page_delay_ms);
    }
    consequence_index = rich4_special_event_consequence_index(
        session->presentation.special_event.logical_id);
    if (assets != NULL && consequence_index >= 0 &&
        session->presentation.special_event.family == GAME_SPECIAL_EVENT_NEWS &&
        (session->presentation.special_event.phase == RICH4_SPECIAL_PRESENT_CONSEQUENCE ||
         (session->presentation.special_event.phase == RICH4_SPECIAL_PRESENT_PAGE &&
          (int32_t)(now - session->presentation.special_event.until) >= 0 && !speech_playing)) &&
        game_asset_ready(assets, &assets->ui.has_news_consequence[consequence_index])) {
        game_special_event_graph_input(
            assets->ui.news_consequence_data[consequence_index],
            assets->ui.news_consequence_size[consequence_index],
            &input->consequence_frame_count,
            &input->consequence_delay_ms);
    }
    if (session->presentation.special_event.phase ==
            RICH4_SPECIAL_PRESENT_FORTUNE_ANIMATION &&
        game_tool_effect_data(
            assets,
            session->presentation.special_event.resolved_id == 7 ?
                533 : 558, &data, &size))
        game_special_event_graph_input(
            data, size, &input->fortune_frame_count,
            &input->fortune_delay_ms);
    if (session->presentation.special_event.phase ==
            RICH4_SPECIAL_PRESENT_CONFINEMENT_ANIMATION &&
        game_tool_effect_data(
            assets,
            session->presentation.special_event.confinement ==
                GAME_CONFINEMENT_PRISON ? 538 : 524,
            &data, &size))
        game_special_event_graph_input(
            data, size, &input->confinement_frame_count,
            &input->confinement_delay_ms);
}

static void game_start_special_event_body(
    GameSession *session, const GamePreviewAssets *assets, Rich4Audio *audio)
{
    GameSpecialEventPresentationState *special = &session->presentation.special_event;
    (void)game_special_event_asset_index(assets, special->resource);
    if (assets != NULL) (void)game_asset_ready(assets, &assets->ui.has_panel[66]);
    special->tick = rich4_sdl1_ticks();
    special->until = special->tick + (special->family == GAME_SPECIAL_EVENT_NEWS ? 2400u : 1600u);
    rich4_sdl_play_special_event_body_voice(
        special->family, special->logical_id, special->resolved_id, audio);
}

void game_begin_special_event_presentation(
    GameSession *session, const GamePreviewAssets *assets, const GameEvent *event,
    Rich4Audio *audio, Uint32 now)
{
    if (!rich4_special_event_begin_from_event(session, event, now)) return;
    game_start_special_event_body(session, assets, audio);
}

int game_update_special_event_presentation(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, const Rich4Config *config,
    Rich4Audio *audio, Uint32 now)
{
    Rich4SpecialEventUpdateInput input;
    Rich4SpecialEventUpdateResult result;
    GameVisibleMapObject area_objects[GAME_MAP_RENDER_QUEUE_CAPACITY];
    GameNewsAreaAnchor area_anchors[GAME_MAP_RENDER_QUEUE_CAPACITY];
    int started;

    if (session == NULL ||
        !session->presentation.special_event.active)
        return 0;
    if ((session->presentation.special_event.phase == RICH4_SPECIAL_PRESENT_REACTION ||
         session->presentation.special_event.phase == RICH4_SPECIAL_PRESENT_PASSIVE_ACTOR_SPEECH ||
         session->presentation.special_event.phase == RICH4_SPECIAL_PRESENT_PASSIVE_VICTIM_SPEECH) &&
        session->presentation.speech.active &&
        game_update_property_speech(session, audio, now))
        return 1;
    game_prepare_special_event_update_input(session, assets, &input, now,
        rich4_audio_speech_playing(audio));
    now = rich4_sdl1_ticks();
    input.speech_active = session->presentation.speech.active;
    input.audio_speech_playing = rich4_audio_speech_playing(audio);
    if (rich4_special_event_prepare_news_area_view(
            session, now, input.audio_speech_playing) && assets != NULL) {
        int background = session->state.stage * GAME_MAPS_PER_STAGE + session->state.map;
        GameViewModelInput view_input;
        GameViewModel view;
        if (background >= 0 && background < GAME_BACKGROUND_COUNT) {
            rich4_sdl1_game_view_model_input_build(
                &session->flow, &session->presentation, &view_input);
            if (rich4_game_view_model_build(&session->state, &view_input,
                    &assets->map.maps[background], &view)) {
                int i;
                int count = game_map_collect_ai_objects(assets, background, &view,
                    session->presentation.dynamic_objects, area_objects,
                    GAME_MAP_RENDER_QUEUE_CAPACITY);
                for (i = 0; i < count; i++) {
                    area_anchors[i].raw_id = (uint16_t)area_objects[i].object_id;
                    area_anchors[i].screen_x = area_objects[i].x;
                    area_anchors[i].screen_y = area_objects[i].y - 40;
                }
                input.news_area_anchors = area_anchors;
                input.news_area_count = count;
                input.news_area_available = 1;
                input.news_area_map = &assets->map.maps[background];
            }
        }
    }
    if (!rich4_special_event_update(session, &input, now, &result))
        return 0;
    if (result.sync_player_to_node)
        game_sync_player_actor_to_node(
            session, assets, settings, result.sync_player, 1);
    switch (result.action) {
    case RICH4_SPECIAL_EVENT_UPDATE_BEGIN:
        game_start_special_event_body(session, assets, audio);
        return 1;
    case RICH4_SPECIAL_EVENT_UPDATE_REACTION:
        started = rich4_sdl_apply_special_event_reaction(
            session, assets, settings, config, audio, now,
            &result.reaction);
        return started ? 1 :
            rich4_special_event_finish_callback(session, now);
    case RICH4_SPECIAL_EVENT_UPDATE_CONSEQUENCE:
        rich4_audio_play_news_consequence(
            audio, session->presentation.special_event.logical_id);
        return 1;
    case RICH4_SPECIAL_EVENT_UPDATE_FORTUNE_ANIMATION:
        started = rich4_sdl_start_special_event_fortune_animation(
            session, assets, audio, now);
        return started ? 1 :
            rich4_special_event_finish_callback(session, now);
    case RICH4_SPECIAL_EVENT_UPDATE_CONFINEMENT_ANIMATION:
        started = rich4_sdl_start_special_event_confinement_animation(
            session, assets, settings, config, audio, now);
        return started ? 1 :
            rich4_special_event_finish_callback(session, now);
    case RICH4_SPECIAL_EVENT_UPDATE_WAIT:
        return 1;
    case RICH4_SPECIAL_EVENT_UPDATE_NONE:
    default:
        return 0;
    }
}

void game_skip_special_event_wait(
    GameSession *session, Rich4Audio *audio, Uint32 now)
{
    Rich4AudioEventQueue *audio_events =
        rich4_app_session_audio_borrow(session);
    rich4_special_event_skip_wait(session, now, audio_events);
    game_session_audio_flush(session, audio);
}
