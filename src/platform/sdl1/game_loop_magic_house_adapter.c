#include "platform/sdl1/game_lazy_assets.h"
#include "app/magic_house_flow.h"
#include "app/game_session.h"
#include "app/game_presentation.h"
#include "app/new_game_setup_flow.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/runtime.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/game_route_api.h"
#include "platform/sdl1/game_loop_audio_events.h"
#include "platform/sdl1/game_loop_turns.h"

static void game_apply_magic_events(
    GameSession *session, Rich4Audio *audio, uint32_t now,
    GameEventQueue *events, const GamePreviewAssets *assets)
{
    GameEvent event;
    if (events == NULL) return;
    while (rich4_game_event_queue_pop(events, &event))
        if (event.type == GAME_EVENT_SPECIAL_EVENT_PRESENTATION_REQUESTED)
            game_begin_special_event_presentation(session, assets, &event, audio, now);
        else if (event.type == GAME_EVENT_GOD_ACQUIRED)
            game_begin_god_presentation(session, assets, &event, now);
}

void game_begin_magic_house(
    GameSession *session, const GameEvent *event,
    Rich4Audio *audio, Uint32 now, int animation_enabled)
{
    Rich4AudioEventQueue *audio_events =
        rich4_app_session_audio_borrow(session);
    rich4_magic_house_begin(
        session, event, now, animation_enabled, audio_events);
    game_session_audio_flush(session, audio);
}

int game_magic_house_mask_at(
    const GamePreviewAssets *assets, int x, int y)
{
    if (assets == NULL || !game_asset_ready(assets, &assets->ui.has_panel_raw[19]) ||
        assets->ui.panel_data[19] == NULL || x < 0 || x >= SCREEN_W ||
        y < 0 || y >= SCREEN_H)
        return 0;
    return assets->ui.panel_data[19][y * SCREEN_W + x];
}

int game_magic_house_choose_facility(
    GameSession *session, Rich4Audio *audio, int choice, Uint32 now)
{
    Rich4AudioEventQueue *audio_events =
        rich4_app_session_audio_borrow(session);
    GameEventQueue events;
    rich4_game_event_queue_reset(&events);
    if (!rich4_magic_house_choose_facility(
            session, choice, now, audio_events, &events)) {
        /* A rejected choice discards whatever it queued, as before. */
        (void)rich4_app_session_audio_release(session);
        return 0;
    }
    game_session_audio_flush(session, audio);
    game_apply_magic_events(session, audio, now, &events, NULL);
    return 1;
}

void game_magic_house_start_phase(
    GameSession *session, Rich4Audio *audio, int phase, Uint32 now)
{
    Rich4AudioEventQueue *audio_events =
        rich4_app_session_audio_borrow(session);
    rich4_magic_house_start_phase(session, phase, now, audio_events);
    game_session_audio_flush(session, audio);
}

void game_magic_house_skip_intro(
    GameSession *session, Rich4Audio *audio, Uint32 now)
{
    Rich4AudioEventQueue *audio_events =
        rich4_app_session_audio_borrow(session);
    rich4_magic_house_skip_intro(session, now, audio_events);
    game_session_audio_flush(session, audio);
}

void game_magic_house_select_action(
    GameSession *session, Rich4Audio *audio, int action, Uint32 now)
{
    Rich4AudioEventQueue *audio_events =
        rich4_app_session_audio_borrow(session);
    rich4_magic_house_select_action(session, action, now, audio_events);
    game_session_audio_flush(session, audio);
}

void game_finish_magic_house(
    GameSession *session, Rich4Audio *audio, int action, Uint32 now)
{
    Rich4AudioEventQueue *audio_events =
        rich4_app_session_audio_borrow(session);
    GameEventQueue events;
    rich4_magic_house_finish(
        session, action, now, audio_events, &events);
    game_session_audio_flush(session, audio);
    game_apply_magic_events(session, audio, now, &events, NULL);
}

int game_update_magic_house(
    GameSession *session, Rich4Audio *audio,
    const GamePreviewAssets *assets, const NewGameSettings *settings,
    Uint32 now)
{
    Rich4AudioEventQueue *audio_events;
    GameEventQueue events;
    int frames = 0, delay = 59;
    int active;
    /* 0x43380A branches to 0x43390B before reading Panel[18..20].
     * AI notice/target effects never play the modal finale. Keep the human
     * modal's existing preparation timing, including surrender. */
    if (session != NULL && session->presentation.magic_house.active &&
        session->presentation.magic_house.phase != RICH4_MAGIC_HOUSE_AI_NOTICE &&
        session->presentation.magic_house.phase != RICH4_MAGIC_HOUSE_TARGET &&
        session->presentation.magic_house.phase != RICH4_MAGIC_HOUSE_FACILITY_CHOICE &&
        assets != NULL && game_asset_ready(assets, &assets->ui.has_panel_graph[20])) {
        frames = game_asset_graph_frames(assets, &assets->ui.panel_data[20], assets->ui.panel_size[20]);
        delay = game_asset_graph_delay(assets, &assets->ui.panel_data[20], assets->ui.panel_size[20]);
    }
    audio_events = rich4_app_session_audio_borrow(session);
    rich4_game_event_queue_reset(&events);
    now = rich4_sdl1_ticks();
    active = rich4_magic_house_update(
        session, frames, delay, now, rich4_audio_speech_playing(audio),
        audio_events, &events);
    if (session != NULL && session->presentation.magic_house.active &&
        session->presentation.magic_house.phase == RICH4_MAGIC_HOUSE_TARGET &&
        session->presentation.magic_house.target_player >= 0 &&
        assets != NULL && settings != NULL) {
        int world_x;
        int world_y;
        game_route_node_xy_for_map(
            assets, settings,
            session->state.players[
                session->presentation.magic_house.target_player].node,
            &world_x, &world_y);
        rich4_presentation_set_view_override(
            &session->presentation, world_x, world_y);
    }
    game_session_audio_flush(session, audio);
    game_apply_magic_events(session, audio, now, &events, assets);
    return active;
}
