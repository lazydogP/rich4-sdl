#include <string.h>

#include "app/game_session.h"
#include "core/game_constants.h"
#include "platform/sdl1/game_point_minigame_view.h"

int build_point_minigame_render_view(const GameSession *session,
                                     GamePointMinigameRenderView *out_view)
{
    int i;

    memset(out_view, 0, sizeof(*out_view));
    if (session == NULL) return 0;
    out_view->game.active = session->presentation.point_minigame.active;
    out_view->game.automatic = session->presentation.point_minigame.automatic;
    out_view->game.music_started = session->presentation.point_minigame.music_started;
    out_view->game.action = session->presentation.point_minigame.action;
    out_view->game.player = session->presentation.point_minigame.player;
    out_view->game.phase = session->presentation.point_minigame.phase;
    out_view->game.ticks_remaining = session->presentation.point_minigame.ticks_remaining;
    out_view->game.score = session->presentation.point_minigame.score;
    out_view->game.result_tier = session->presentation.point_minigame.result_tier;
    out_view->game.animation_frame = session->presentation.point_minigame.animation_frame;
    out_view->game.animation_delay = session->presentation.point_minigame.animation_delay;
    out_view->game.next_tick = session->presentation.point_minigame.next_tick;
    out_view->game.finish_until = session->presentation.point_minigame.finish_until;
    memcpy(out_view->game.counters, session->presentation.point_minigame.counters,
           sizeof(out_view->game.counters));
    memcpy(out_view->game.penguin_treasure,
           session->presentation.point_minigame.penguin_treasure,
           sizeof(out_view->game.penguin_treasure));
    out_view->game.penguin_player_cell = session->presentation.point_minigame.penguin_player_cell;
    out_view->game.penguin_target_cell = session->presentation.point_minigame.penguin_target_cell;
    out_view->game.penguin_next_cell = session->presentation.point_minigame.penguin_next_cell;
    out_view->game.penguin_move_frame = session->presentation.point_minigame.penguin_move_frame;
    out_view->game.penguin_dig_frame = session->presentation.point_minigame.penguin_dig_frame;
    out_view->game.penguin_pickup_type = session->presentation.point_minigame.penguin_pickup_type;
    out_view->game.penguin_pickup_frame = session->presentation.point_minigame.penguin_pickup_frame;
    out_view->game.balloon_slow_ticks = session->presentation.point_minigame.balloon_slow_ticks;
    out_view->game.balloon_speed_mode = session->presentation.point_minigame.balloon_speed_mode;
    out_view->game.catcher_x = session->presentation.point_minigame.catcher_x;
    out_view->game.catcher_direction = session->presentation.point_minigame.catcher_direction;
    out_view->game.catcher_frame = session->presentation.point_minigame.catcher_frame;
    out_view->game.catcher_source_x = session->presentation.point_minigame.catcher_source_x;
    out_view->game.catcher_source_direction = session->presentation.point_minigame.catcher_source_direction;
    out_view->game.catcher_secondary_active = session->presentation.point_minigame.catcher_secondary_active;
    out_view->game.catcher_secondary_frame = session->presentation.point_minigame.catcher_secondary_frame;
    out_view->game.catcher_bomb_active = session->presentation.point_minigame.catcher_bomb_active;
    if (out_view->game.action == 8) {
        const GameCatcherMinigameState *c = &session->presentation.point_minigame.catcher;
        out_view->game.catcher_source_x = c->source_draw_x;
        out_view->game.catcher_source_chunk = c->source_draw_chunk;
        out_view->game.catcher_secondary_x = c->secondary_x;
        out_view->game.catcher_secondary_active = c->secondary_draw_frame >= 0;
        out_view->game.catcher_secondary_frame = c->secondary_draw_frame;
        out_view->game.catcher_explosion_frame = c->explosion_frame - 1;
        if (c->stage == 2 && c->explosion_frame == 8 &&
            out_view->game.phase == RICH4_POINT_MINIGAME_PLAY)
            out_view->game.catcher_bomb_active = 1;
    }
    for (i = 0; i < GAME_POINT_MINIGAME_OBJECT_COUNT; i++) {
        const PointMinigameObject *o = out_view->game.action == 8
            ? &session->presentation.point_minigame.catcher.draw_objects[i]
            : &session->presentation.point_minigame.object[i];
        out_view->game.object[i].active = o->active;
        out_view->game.object[i].x = o->x;
        out_view->game.object[i].y = o->y;
        out_view->game.object[i].type = o->type;
        out_view->game.object[i].frame = o->frame;
        out_view->game.object[i].velocity_x = o->velocity_x;
        out_view->game.object[i].velocity_y = o->velocity_y;
    }
    if (session->presentation.point_minigame.player >= 0 &&
        session->presentation.point_minigame.player < session->state.player_count)
        out_view->player_character =
            session->state.players[session->presentation.point_minigame.player].character;
    return 1;
}
