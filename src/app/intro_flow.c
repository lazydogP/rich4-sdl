#include "app/intro_flow.h"

#include <stdio.h>
#include <string.h>

#include "app/route_geometry_flow.h"
#include "view/view_constants.h"

static int intro_wrap_index(int value, int count)
{
    if (count <= 0) return 0;
    value %= count;
    if (value < 0) value += count;
    return value;
}

static int intro_player_count(const GameSession *session)
{
    if (session == NULL || session->state.player_count <= 0) return 1;
    return session->state.player_count > GAME_MAX_PLAYERS
        ? GAME_MAX_PLAYERS : session->state.player_count;
}

int rich4_intro_jump_character_index(const GamePlayerState *player,
                                     int fallback)
{
    int character = player != NULL ? player->character : fallback;
    if (character < 0 || character >= GAME_JUMP_CHARACTER_COUNT)
        character = intro_wrap_index(character, GAME_JUMP_CHARACTER_COUNT);
    return character;
}

void rich4_intro_begin(GameSession *session)
{
    GameIntroPresentationState *intro;
    if (session == NULL) return;
    intro = &session->presentation.intro;
    intro->active = 1;
    intro->interrupt_requested = 0;
    intro->player = 0;
    rich4_animation_clock_reset(&session->presentation.animation_clock[RICH4_ANIMATION_INTRO]);
    intro->frame = 0;
    intro->tick = 0;
    intro->jump_sound_pending = 0;
    /* The door sound is consumed by the platform audio adapter immediately
     * before the first door graph frame. */
    intro->door_sound_pending = 1;
}

void rich4_intro_set_avi_selection(GameSession *session, int stage, int map,
                                   const char *avi_name)
{
    GameIntroPresentationState *intro;
    if (session == NULL) return;
    intro = &session->presentation.intro;
    intro->avi_attempted = 0;
    intro->avi_skipped_no_backend = 0;
    intro->avi_original_stage = stage;
    intro->avi_original_map = map;
    intro->avi_name[0] = '\0';
    if (avi_name == NULL) return;
    snprintf(intro->avi_name, sizeof(intro->avi_name), "%s", avi_name);
    intro->avi_attempted = 1;
    intro->avi_skipped_no_backend = 1;
}

Rich4IntroStage rich4_intro_stage_for_frame(const Rich4IntroTiming *timing,
                                             const GameSession *session,
                                             int intro_frame)
{
    Rich4IntroStage stage;
    int frame;
    int frames;
    int i;
    int count = intro_player_count(session);

    memset(&stage, 0, sizeof(stage));
    stage.kind = RICH4_INTRO_STAGE_DONE;
    stage.player_index = -1;
    stage.static_player_index = -1;
    stage.frame_count = 1;
    stage.delay_ms = GAME_INTRO_DEFAULT_FRAME_MS;
    if (timing == NULL) return stage;

    frame = intro_frame;
    frames = timing->window_frame_count > 0
        ? timing->window_frame_count : 1;
    if (frame < frames) {
        stage.kind = RICH4_INTRO_STAGE_WINDOW;
        stage.player_index = 0;
        stage.graph_frame = frame;
        stage.frame_count = frames;
        stage.delay_ms = timing->window_delay_ms > 0
            ? timing->window_delay_ms : GAME_INTRO_DEFAULT_FRAME_MS;
        return stage;
    }
    frame -= frames;
    for (i = 0; i < count; i++) {
        frames = timing->actor_frame_count[0][i] > 0
            ? timing->actor_frame_count[0][i] : 1;
        if (frame < frames) {
            stage.kind = RICH4_INTRO_STAGE_TAKEOFF;
            stage.player_index = i;
            stage.static_player_index = i + 1 < count ? i + 1 : -1;
            stage.graph_frame = frame;
            stage.frame_count = frames;
            stage.delay_ms = timing->actor_delay_ms[0][i] > 0
                ? timing->actor_delay_ms[0][i] : GAME_INTRO_DEFAULT_FRAME_MS;
            return stage;
        }
        frame -= frames;
    }
    for (i = 0; i < count; i++) {
        frames = timing->actor_frame_count[1][i] > 0
            ? timing->actor_frame_count[1][i] : 1;
        if (frame < frames) {
            stage.kind = RICH4_INTRO_STAGE_PARACHUTE;
            stage.player_index = i;
            stage.graph_frame = frame;
            stage.frame_count = frames;
            stage.delay_ms = timing->actor_delay_ms[1][i] > 0
                ? timing->actor_delay_ms[1][i] : GAME_INTRO_DEFAULT_FRAME_MS;
            return stage;
        }
        frame -= frames;
    }
    return stage;
}

int rich4_intro_total_frames(const Rich4IntroTiming *timing,
                             const GameSession *session)
{
    int count = intro_player_count(session);
    int total;
    int i;

    if (timing == NULL) return 1;
    total = timing->window_frame_count > 0 ? timing->window_frame_count : 1;
    for (i = 0; i < count; i++)
        total += timing->actor_frame_count[0][i] > 0
            ? timing->actor_frame_count[0][i] : 1;
    for (i = 0; i < count; i++)
        total += timing->actor_frame_count[1][i] > 0
            ? timing->actor_frame_count[1][i] : 1;
    return total;
}

void rich4_intro_finish(GameSession *session)
{
    int i;
    int current;

    if (session == NULL || !session->presentation.intro.active) return;
    for (i = 0; i < session->state.player_count && i < GAME_MAX_PLAYERS; i++) {
        session->presentation.actors[i].intro_visible = 0;
        session->presentation.actors[i].walk_frame = 0;
    }
    session->presentation.intro.player = session->state.player_count;
    session->presentation.intro.active = 0;
    session->presentation.intro.interrupt_requested = 0;
    session->presentation.has_view_override = 0;
    current = intro_wrap_index(session->state.current_player,
                               session->state.player_count);
    if (session->state.player_count > 0 && current < GAME_MAX_PLAYERS)
        session->presentation.actors[current].dirty_flags |=
            GAME_PLAYER_DIRTY_TURN;
}

void rich4_intro_finish_at_tick(GameSession *session, uint32_t now)
{
    if (session == NULL || !session->presentation.intro.active) return;
    /* 0x402F99/0x402FA1 reseeds once when the arrival presentation returns. */
    session->state.ai_rng_state = now + session->rng_clock_offset;
    rich4_intro_finish(session);
}

int rich4_intro_finish_if_requested_at_tick(GameSession *session, uint32_t now)
{
    if (session == NULL || !session->presentation.intro.active ||
        !session->presentation.intro.interrupt_requested) return 0;
    rich4_intro_finish_at_tick(session, now);
    return 1;
}

void rich4_intro_request_skip(GameSession *session)
{
    if (session != NULL && session->presentation.intro.active)
        session->presentation.intro.interrupt_requested = 1;
}

int rich4_intro_finish_if_requested(GameSession *session)
{
    if (session == NULL || !session->presentation.intro.active ||
        !session->presentation.intro.interrupt_requested)
        return 0;
    rich4_intro_finish(session);
    return 1;
}

void rich4_intro_update(GameSession *session, const Rich4IntroTiming *timing,
                        const MapData *map, uint32_t now)
{
    int intro_total;
    int intro_player;

    if (session == NULL || timing == NULL ||
        !session->presentation.intro.active)
        return;
    intro_total = rich4_intro_total_frames(timing, session);
    intro_player = intro_wrap_index(session->presentation.intro.player,
                                    session->state.player_count);
    if (map != NULL && intro_player >= 0 &&
        intro_player < session->state.player_count &&
        rich4_route_node_is_valid(map,
            session->state.players[intro_player].node)) {
        rich4_route_node_xy_for_map(
            map, session->state.players[intro_player].node,
            &session->presentation.view_world_x,
            &session->presentation.view_world_y);
        session->presentation.has_view_override = 1;
    }
    if (session->presentation.intro.tick == 0)
        session->presentation.intro.tick = now;
    while (session->presentation.intro.active) {
        Rich4IntroStage intro_stage = rich4_intro_stage_for_frame(
            timing, session, session->presentation.intro.frame);
        uint32_t delay_ms = (uint32_t)(intro_stage.delay_ms > 0
            ? intro_stage.delay_ms : GAME_INTRO_DEFAULT_FRAME_MS);
        if (!rich4_animation_clock_step(
                &session->presentation.animation_clock[RICH4_ANIMATION_INTRO],
                now, (int)delay_ms)) break;
        session->presentation.intro.frame++;
        if (intro_stage.kind == RICH4_INTRO_STAGE_TAKEOFF &&
            intro_stage.graph_frame < 19 &&
            intro_stage.graph_frame + 1 >= 19)
            session->presentation.intro.jump_sound_pending++;
        session->presentation.intro.tick = now;
        if (session->presentation.intro.frame >= intro_total) {
            rich4_intro_finish_at_tick(session, now);
            break;
        }
        intro_stage = rich4_intro_stage_for_frame(
            timing, session, session->presentation.intro.frame);
        if (intro_stage.player_index >= 0)
            session->presentation.intro.player = intro_stage.player_index;
        else if (intro_stage.static_player_index >= 0)
            session->presentation.intro.player =
                intro_stage.static_player_index;
        else
            session->presentation.intro.player = 0;
        break;
    }
}
