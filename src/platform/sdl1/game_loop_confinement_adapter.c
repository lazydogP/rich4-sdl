#include "app/confinement_flow.h"
#include "app/game_session.h"
#include "app/game_presentation.h"
#include "app/game_speech_flow.h"
#include "app/new_game_setup_flow.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "core/confinement.h"
#include "core/game_event_payload.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/game_loop_audio_events.h"
#include "platform/sdl1/game_route_api.h"

int game_sync_player_actor_to_node(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, int player_index, int center_view)
{
    int background_index;
    const MapData *map;
    if (session == NULL || assets == NULL || settings == NULL ||
        player_index < 0 || player_index >= session->state.player_count) return 0;
    background_index = settings->stage * GAME_MAPS_PER_STAGE + settings->map;
    if (background_index < 0 || background_index >= GAME_BACKGROUND_COUNT ||
            assets->map.maps[background_index].route_count <= 0)
        return 0;
    map = &assets->map.maps[background_index];
    if (!rich4_confinement_sync_player_actor_to_node(
            session, map, player_index))
        return 0;
    if (center_view) {
        const GameActorPresentationState *actor =
            &session->presentation.actors[player_index];
        game_focus_view_at(session, actor->visual_x, actor->visual_y);
    }
    return 1;
}

static int game_confinement_event_player(
    const GameSession *session, const GameEvent *event)
{
    int player;
    int status = -1;
    if (session == NULL || event == NULL ||
        event->type != GAME_EVENT_STATUS_CHANGED) return -1;
    player = event->player;
    if (rich4_game_event_status_subject(event).kind == GAME_DOMAIN_PLAYER)
        player = rich4_game_event_status_subject(event).id;
    if (rich4_game_event_status_subject(event).kind == GAME_DOMAIN_STATUS)
        status = rich4_game_event_status_subject(event).id;
    if (rich4_game_event_status_target(event).kind == GAME_DOMAIN_STATUS)
        status = rich4_game_event_status_target(event).id;
    if (status < 0) {
        if (rich4_game_event_status_offset(event) == 52) status = GAME_STATUS_PRISON;
        else if (rich4_game_event_status_offset(event) == 53)
            status = GAME_STATUS_HOSPITAL;
    }
    if (player < 0 || player >= session->state.player_count) return -1;
    if (status == GAME_STATUS_PRISON &&
        (session->state.players[player].days_in_prison & 0x7f) != 0) return player;
    if (status == GAME_STATUS_HOSPITAL &&
        (session->state.players[player].days_in_hospital & 0x7f) != 0) return player;
    return -1;
}

void game_sync_confinement_event(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, const GameEvent *event)
{
    int player = game_confinement_event_player(session, event);
    if (player >= 0 && session->presentation.special_event.active &&
        session->presentation.special_event.confinement != 0) return;
    if (player >= 0)
        game_sync_player_actor_to_node(session, assets, settings, player, 1);
}

int game_center_view_on_player_status(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, const GamePlayerState *player)
{
    int background_index;
    int world_x;
    int world_y;
    if (session == NULL || assets == NULL || settings == NULL || player == NULL)
        return 0;
    background_index = settings->stage * GAME_MAPS_PER_STAGE + settings->map;
    if (background_index < 0 || background_index >= GAME_BACKGROUND_COUNT ||
        player->node <= 0 || (size_t)player->node >=
            assets->map.maps[background_index].route_count) return 0;
    if (!rich4_confinement_anchor_xy(
            &assets->map.maps[background_index],
            player->confinement_actor_type, &world_x, &world_y))
        game_route_node_xy_for_map(assets, settings, player->node,
                                   &world_x, &world_y);
    /* This can precede actor sync and therefore use an original-player
     * snapshot while the live player already has a facility node. */
    rich4_presentation_set_view_override(
        &session->presentation, world_x, world_y);
    return 1;
}

void game_begin_confinement_visit(
    GameSession *session, const GameEvent *event,
    Rich4Audio *audio, Uint32 now)
{
    Rich4AudioEventQueue *audio_events =
        rich4_app_session_audio_borrow(session);
    rich4_confinement_begin(session, event, now, audio_events);
    game_session_audio_flush(session, audio);
}

void game_confinement_visit_start_hospital_exit(
    GameSession *session, Rich4Audio *audio, Uint32 now)
{
    Rich4AudioEventQueue *audio_events =
        rich4_app_session_audio_borrow(session);
    rich4_confinement_visit_start_hospital_exit(
        session, now, audio_events);
    game_session_audio_flush(session, audio);
}

void game_confinement_visit_skip_speech(
    GameSession *session, Rich4Audio *audio, Uint32 now)
{
    Rich4AudioEventQueue *audio_events =
        rich4_app_session_audio_borrow(session);
    rich4_confinement_visit_skip_speech(session, now, audio_events);
    game_session_audio_flush(session, audio);
}

int game_update_confinement_visit(
    GameSession *session, Rich4Audio *audio, Uint32 now)
{
    Rich4AudioEventQueue *audio_events =
        rich4_app_session_audio_borrow(session);
    int active = rich4_confinement_update(
        session, now, rich4_audio_speech_playing(audio), audio_events);
    game_session_audio_flush(session, audio);
    return active;
}

void game_confinement_visit_start_speech(
    GameSession *session, Rich4Audio *audio,
    const Rich4CharacterSpeech *speech, int special_actor, Uint32 now)
{
    Rich4AudioEventQueue *audio_events =
        rich4_app_session_audio_borrow(session);
    rich4_confinement_visit_start_speech(
        session, speech, special_actor, now, audio_events);
    game_session_audio_flush(session, audio);
}
