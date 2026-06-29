#include "platform/sdl1/game_lazy_assets.h"
#include <SDL/SDL.h>

#include "app/config_types.h"
#include "app/game_session.h"
#include "app/special_presentation_flow.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "core/character_speech_rules.h"
#include "core/game_constants.h"
#include "core/game_event_payload.h"
#include "core/special_event.h"
#include "data/character_speech.h"
#include "data/special_event_text.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/render/game_scene_view.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/runtime.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/game_special_event_internal.h"

int game_special_event_asset_index(
    const GamePreviewAssets *assets, int resource)
{
    int index;
    if (assets == NULL) return -1;
    for (index = 0; index < GAME_SPECIAL_EVENT_ASSET_COUNT; index++)
        if (assets->ui.special_event_resource[index] == resource &&
            game_asset_ready(assets, &assets->ui.has_special_event[index]))
            return index;
    return -1;
}

void rich4_sdl_play_special_event_body_voice(
    int family, int logical_id, int resolved_id, Rich4Audio *audio)
{
    const char *marked_text;
    Rich4PlayerSayText decoded;
    marked_text = family == GAME_SPECIAL_EVENT_NEWS
        ? rich4_news_text_big5(logical_id)
        : rich4_fortune_text_big5(resolved_id);
    rich4_player_say_decode(marked_text, -1, &decoded);
    if (decoded.speaking_resource >= 0)
        rich4_audio_play_speech(audio, decoded.speaking_resource);
}

int rich4_sdl_start_special_event_confinement_animation(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, const Rich4Config *config,
    Rich4Audio *audio, Uint32 now)
{
    const unsigned char *data;
    size_t size;
    int resource;
    int effect;
    int frames;
    int delay;
    int player;
    if (session == NULL) return 0;
    player = session->presentation.special_event.reaction_player;
    resource = session->presentation.special_event.confinement ==
        GAME_CONFINEMENT_PRISON ? 538 : 524;
    effect = session->presentation.special_event.confinement ==
        GAME_CONFINEMENT_PRISON ? 94 : 92;
    if (!session->presentation.special_event.first_confinement ||
        config == NULL || config->animation == 0 ||
        !game_tool_effect_data(assets, resource, &data, &size)) {
        rich4_special_event_set_reaction_stage(session, 3);
        game_sync_player_actor_to_node(
            session, assets, settings, player, 1);
        return rich4_special_event_start_confinement_final_speech(
            session, player, now);
    }
    frames = game_graph_frame_count_data(data, size);
    delay = game_graph_frame_delay_ms_data(data, size);
    if (frames <= 0) {
        rich4_special_event_set_reaction_stage(session, 3);
        game_sync_player_actor_to_node(
            session, assets, settings, player, 1);
        return rich4_special_event_start_confinement_final_speech(
            session, player, now);
    }
    if (delay <= 0) delay = 71;
    now = rich4_sdl1_ticks();
    if (!rich4_special_event_begin_timed_phase(
            session, RICH4_SPECIAL_PRESENT_CONFINEMENT_ANIMATION,
            frames, delay, now,
            session->presentation.special_event.confinement ==
                GAME_CONFINEMENT_PRISON ? 18 : 30))
        return 0;
    rich4_audio_play_tool_effect(audio, effect);
    return 1;
}

int rich4_sdl_start_special_event_fortune_animation(
    GameSession *session, const GamePreviewAssets *assets,
    Rich4Audio *audio, Uint32 now)
{
    const unsigned char *data;
    size_t size;
    int id;
    int resource;
    int effect;
    int frames;
    int delay;
    if (session == NULL) return 0;
    id = session->presentation.special_event.resolved_id;
    if (id != 6 && id != 7) return 0;
    resource = id == 7 ? 533 : 558;
    effect = id == 7 ? 84 : 96;
    if (!game_tool_effect_data(assets, resource, &data, &size))
        return 0;
    frames = game_graph_frame_count_data(data, size);
    delay = game_graph_frame_delay_ms_data(data, size);
    if (frames <= 0) return 0;
    if (delay <= 0) delay = 71;
    now = rich4_sdl1_ticks();
    if (!rich4_special_event_begin_timed_phase(
            session, RICH4_SPECIAL_PRESENT_FORTUNE_ANIMATION,
            frames, delay, now, id == 7 ? 28 : 20))
        return 0;
    rich4_audio_play_tool_effect(audio, effect);
    return 1;
}

int rich4_sdl_apply_special_event_reaction(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, const Rich4Config *config,
    Rich4Audio *audio, Uint32 now,
    const Rich4SpecialEventReactionResult *result)
{
    const GamePlayerState *original_player;
    if (session == NULL || result == NULL) return 0;
    if (result->player >= 0 && result->center_original_player) {
        original_player = rich4_special_event_original_player(
            session, result->player);
        if (original_player != NULL)
            game_center_view_on_player_status(
                session, assets, settings, original_player);
    }
    if (result->player >= 0 && result->sync_player_to_node)
        game_sync_player_actor_to_node(
            session, assets, settings, result->player, 1);
    if (result->action == RICH4_SPECIAL_EVENT_REACTION_CONFINEMENT_ANIMATION)
        return rich4_sdl_start_special_event_confinement_animation(
            session, assets, settings, config, audio, now);
    if (result->action == RICH4_SPECIAL_EVENT_REACTION_FORTUNE_ANIMATION)
        return rich4_sdl_start_special_event_fortune_animation(
            session, assets, audio, now);
    return result->action == RICH4_SPECIAL_EVENT_REACTION_SPEECH;
}
