#include "app/point_minigame_flow.h"
#include "app/audio_event.h"
#include "app/game_speech_flow.h"
#include "core/character_speech_rules.h"
#include "core/game_event_payload.h"
#include "core/rng.h"
#include "core/route_event_executor.h"

#include <stdlib.h>
#include <string.h>

/* App-owned point-minigame state machine. It borrows GameSession and emits
 * Rich4AudioEvent values; SDL resource geometry and playback stay outside. */

static void rich4_point_minigame_emit(
    Rich4AudioEventQueue *audio_events, Rich4AudioEventType type, int value)
{
    if (audio_events != NULL)
        rich4_audio_event_emit(audio_events, type, 1, value);
}

int rich4_point_minigame_cell_valid(int column, int row)
{
    static const unsigned short valid_rows[9] = {
        0x078, 0x0fe, 0x1fe, 0x1ff, 0x1ef,
        0x1ff, 0x0ff, 0x0fe, 0x03c
    };
    return column >= 0 && column < 9 && row >= 0 && row < 9 &&
           (valid_rows[row] & (1u << column)) != 0;
}

int rich4_point_minigame_penguin_next_cell(
    int current_cell, int target_cell)
{
    int current_column = current_cell % 9;
    int current_row = current_cell / 9;
    int target_column = target_cell % 9;
    int target_row = target_cell / 9;
    int best_cell = current_cell;
    int best_distance = 0x7fffffff;
    int row_delta;
    int column_delta;
    for (row_delta = -1; row_delta <= 1; row_delta++) {
        for (column_delta = -1; column_delta <= 1; column_delta++) {
            int row;
            int column;
            int distance;
            if (row_delta == 0 && column_delta == 0) continue;
            row = current_row + row_delta;
            column = current_column + column_delta;
            if (!rich4_point_minigame_cell_valid(column, row)) continue;
            distance = abs(target_row - row) + abs(target_column - column);
            if (distance < best_distance) {
                best_distance = distance;
                best_cell = row * 9 + column;
            }
        }
    }
    return best_cell;
}

static void game_point_minigame_clear(struct PointMinigameState *game)
{
    int drop = game->catcher.drop_frame, anchor = game->catcher.mouse_anchor;
    memset(game, 0, sizeof(*game));
    /* Original globals 0x48BD40 / 0x47504F outlive a single visit. */
    game->catcher.drop_frame = drop;
    game->catcher.mouse_anchor = anchor;
}

static void game_finish_point_minigame(
    GameSession *session, Rich4AudioEventQueue *audio_events)
{
    GameEventQueue events;
    GameEvent event;
    RouteEventExecutionResult result;
    if (session == NULL || !session->presentation.point_minigame.active) return;
    if (session->presentation.point_minigame.automatic >=
            RICH4_POINT_MINIGAME_AUTOMATIC_RESOLVED_FIRST_SPEECH) {
        /* 0x415457: the automatic core event already awarded the points. */
        game_point_minigame_clear(&session->presentation.point_minigame);
        return;
    }
    rich4_game_event_queue_reset(&events);
    result = rich4_route_event_executor_finish_point_minigame(
        &session->flow.route_events, &session->state,
        session->presentation.point_minigame.score, &events);
    if (result == ROUTE_EVENT_EXECUTION_INVALID) return;
    while (rich4_game_event_queue_pop(&events, &event))
        if (event.type == GAME_EVENT_ROUTE_EVENT_RESOLVED)
            session->presentation.route_interaction.active = 0;
    rich4_point_minigame_emit(audio_events, RICH4_AUDIO_EVENT_POINT_MINIGAME_STOP_EFFECT, 24);
    game_point_minigame_clear(&session->presentation.point_minigame);
}

static void game_point_minigame_begin_result(
    GameSession *session, Rich4AudioEventQueue *audio_events, uint32_t now)
{
    struct PointMinigameState *game;
    if (session == NULL) return;
    game = &session->presentation.point_minigame;
    if (!game->active || game->phase == RICH4_POINT_MINIGAME_RESULT)
        return;
    if (game->action == 6) {
        game->score = 5 * game->counters[1] +
                      12 * game->counters[2] +
                       8 * game->counters[3] +
                      20 * game->counters[4];
        if (game->score < 40) {
            game->result_tier = 5;
            rich4_point_minigame_emit(audio_events, RICH4_AUDIO_EVENT_POINT_MINIGAME_EFFECT, 14);
        } else if (game->score > 55) {
            game->result_tier = 4;
            rich4_point_minigame_emit(audio_events, RICH4_AUDIO_EVENT_POINT_MINIGAME_EFFECT, 13);
        } else {
            game->result_tier = 6;
        }
    } else if (game->action == 8) {
        game->score = 10 * game->counters[0] +
                       5 * game->counters[1] +
                       3 * game->counters[2] +
                           game->counters[3];
        if (game->result_tier != 4) {
            game->catcher_direction = 0;
            game->result_tier = game->score < 40 ? 1 :
                                game->score < 50 ? 2 :
                                game->score < 60 ? 0 : 3;
        }
    }
    if (game->action != 8 && game->score > 999) game->score = 999;
    game->phase = RICH4_POINT_MINIGAME_RESULT;
    game->animation_frame = 0;
    game->next_tick = now;
    game->finish_until = now + 2000;
}

static void game_point_minigame_place_penguin_treasure(
    GameSession *session, struct PointMinigameState *game)
{
    static const int treasure_count[5] = { 3, 12, 3, 9, 1 };
    int cells[81];
    int available = 0;
    int row;
    int column;
    int type;
    for (row = 0; row < 9; row++)
        for (column = 0; column < 9; column++)
            if (rich4_point_minigame_cell_valid(column, row))
                cells[available++] = row * 9 + column;
    for (type = 1; type <= 5; type++) {
        int count;
        for (count = 0; count < treasure_count[type - 1] &&
                        available > 0; count++) {
            int chosen = rich4_rng_range_at(&session->state.ai_rng_state, available);
            game->penguin_treasure[cells[chosen]] = type;
            cells[chosen] = cells[--available];
        }
    }
}

void rich4_point_minigame_begin(
    GameSession *session, const GameEvent *event, uint32_t now,
    int animation_enabled, Rich4AudioEventQueue *audio_events)
{
    struct PointMinigameState *game;
    int action;
    int resolved;
    (void)audio_events;
    if (session == NULL || event == NULL) return;
    resolved = event->type == GAME_EVENT_POINT_MINIGAME_AUTOMATIC_RESOLVED;
    action = resolved ? rich4_game_event_subject(event)
                      : rich4_game_event_point_action(event);
    if (action < 6 || action > 8 ||
        (resolved && (event->player < 0 ||
                      event->player >= session->state.player_count)))
        return;
    game = &session->presentation.point_minigame;
    game_point_minigame_clear(game);
    game->active = 1;
    game->action = action;
    game->player = resolved ? event->player
                           : session->flow.route_events.state.execution_actor;
    if (resolved || !animation_enabled) {
        /*
         * 0x4154DC/0x4155FC jump to 0x415215:0x415457 when presentation
         * is disabled.  The shared block returns rand()%20+50, displays
         * “得點券%d點” for 2000 ms, then chooses one of the first two
         * character speech entries.  It does not enter any of the three
         * interactive callbacks.
         */
        game->automatic = resolved
            ? RICH4_POINT_MINIGAME_AUTOMATIC_RESOLVED_FIRST_SPEECH +
                  (rich4_game_event_value2(event) & 1)
            : RICH4_POINT_MINIGAME_AUTOMATIC_UNRESOLVED;
        game->score = resolved ? rich4_game_event_value(event)
                              : 50 + rich4_rng_range_at(&session->state.ai_rng_state, 20);
        game->phase = RICH4_POINT_MINIGAME_RESULT;
        game->next_tick = now;
        game->finish_until = now + 2000;
        return;
    }
    rich4_animation_clock_reset(&session->presentation.animation_clock[RICH4_ANIMATION_POINT_INTRO]);
    game->phase = animation_enabled
        ? RICH4_POINT_MINIGAME_INTRO : RICH4_POINT_MINIGAME_PLAY;
    game->ticks_remaining = game->action == 8 ? 360 : 150;
    game->next_tick = now;
    game->penguin_player_cell = 6 * 9 + 2;
    game->penguin_target_cell = -1;
    game->penguin_next_cell = -1;
    game->catcher_x = 320;
    game->catcher_source_x = 110;
    game->catcher_source_direction = 3;
    game->catcher_direction = 0;
    if (game->action == 8) {
        static const unsigned char move_frames[12] = {
            10, 10, 10, 14, 10, 13, 10, 11, 13, 11, 10, 11
        };
        int character = session->state.players[game->player].character;
        game->catcher.move_frames = move_frames[
            character >= 0 && character < 12 ? character : 0];
        game->catcher.mouse_warp = -1;
        game->catcher.mouse_initialized = 1;
        game->catcher.source_frame = 4;
        game->catcher.secondary_draw_frame = -1;
        game->catcher.source_draw_x = 110;
        game->catcher.source_draw_chunk = 7;
        game->catcher.wait_ticks = 10;
        game->phase = RICH4_POINT_MINIGAME_READY;
        game->next_tick = now + 50;
    }
    if (game->action == 6)
        game_point_minigame_place_penguin_treasure(session, game);
}

static int game_point_minigame_spawn_balloon(
    GameSession *session, struct PointMinigameState *game, Rich4AudioEventQueue *audio_events)
{
    static const unsigned char special_type[10] = {
        9, 9, 10, 10, 10, 10, 10, 11, 11, 11
    };
    int slot;
    int random_value;
    int type = -1;
    int available_x[8];
    int available_count = 0;
    int x;
    for (slot = 0; slot < GAME_POINT_MINIGAME_OBJECT_COUNT; slot++)
        if (!game->object[slot].active) break;
    if (slot == GAME_POINT_MINIGAME_OBJECT_COUNT) return 0;
    random_value = rich4_rng_range_at(&session->state.ai_rng_state, 1000);
    if (random_value < 20)
        type = random_value / 4;
    else if (random_value < 28)
        type = ((27 - random_value) >> 1) + 5;
    else if (random_value < 30)
        type = special_type[rich4_rng_range_at(&session->state.ai_rng_state, 10)];
    if (type < 0) return 0;
    for (x = 40; x < 640; x += 80) {
        int used = 0;
        int i;
        for (i = 0; i < GAME_POINT_MINIGAME_OBJECT_COUNT; i++)
            if (game->object[i].active &&
                game->object[i].x == x &&
                game->object[i].y > 300) {
                used = 1;
                break;
            }
        if (!used) available_x[available_count++] = x;
    }
    if (available_count == 0) return 0;
    game->object[slot].active = 1;
    game->object[slot].x =
        available_x[rich4_rng_range_at(&session->state.ai_rng_state, available_count)];
    game->object[slot].y = 420;
    game->object[slot].type = type;
    game->object[slot].frame = 0;
    rich4_point_minigame_emit(audio_events, RICH4_AUDIO_EVENT_POINT_MINIGAME_EFFECT, 19);
    return 1;
}

static int game_point_minigame_spawn_catcher_object(
    GameSession *session, struct PointMinigameState *game, int x, int bomb)
{
    int slot;
    int random_value;
    int type;
    for (slot = 0; slot < GAME_POINT_MINIGAME_OBJECT_COUNT; slot++)
        if (!game->object[slot].active) break;
    if (slot == GAME_POINT_MINIGAME_OBJECT_COUNT) return 0;
    if (bomb) {
        type = 4;
        game->catcher.bombs++;
    } else {
        random_value = rich4_rng_range_at(&session->state.ai_rng_state, 20);
        type = random_value < 9 ? 3 :
               random_value < 15 ? 2 :
               random_value < 18 ? 1 : 0;
    }
    game->object[slot].active = 1;
    game->object[slot].x = x;
    game->object[slot].y = 100;
    game->object[slot].type = type;
    game->object[slot].frame = 0;
    game->object[slot].velocity_y = -16;
    game->object[slot].velocity_x =
        (int)((double)(x - 320) / 210.0 * 260.0 - (x - 320));
    return 1;
}

int rich4_point_minigame_catcher_draw_x(
    const struct PointMinigameObject *object)
{
    float progress;
    if (object == NULL) return 0;
    progress = (float)((double)(object->y > 130 ? object->y - 130 : 0) / 250.0);
    return object->x + (int)((double)object->velocity_x * progress);
}

static void game_update_penguin_minigame(
    GameSession *session, Rich4AudioEventQueue *audio_events, uint32_t now)
{
    struct PointMinigameState *game = &session->presentation.point_minigame;
    if (game->penguin_target_cell >= 0) {
        if (game->penguin_player_cell !=
                game->penguin_target_cell &&
            ++game->penguin_move_frame >= 4) {
            game->penguin_player_cell = game->penguin_next_cell;
            game->penguin_move_frame = 0;
            if (game->penguin_player_cell !=
                game->penguin_target_cell)
                game->penguin_next_cell =
                    rich4_point_minigame_penguin_next_cell(
                        game->penguin_player_cell,
                        game->penguin_target_cell);
        }
        if (game->penguin_player_cell ==
                game->penguin_target_cell &&
            game->penguin_dig_frame < 4) {
            if (game->penguin_dig_frame == 0)
                rich4_point_minigame_emit(audio_events, RICH4_AUDIO_EVENT_POINT_MINIGAME_EFFECT, 11);
            game->penguin_dig_frame++;
        } else if (game->penguin_player_cell ==
                       game->penguin_target_cell) {
            int type =
                game->penguin_treasure[game->penguin_target_cell];
            game->penguin_player_cell = game->penguin_target_cell;
            game->penguin_treasure[game->penguin_target_cell] = 0;
            game->penguin_pickup_type = type;
            game->penguin_pickup_frame++;
            if (type > 0 && game->penguin_pickup_frame == 1)
                game->counters[type - 1]++;
            if (game->penguin_pickup_frame >= 4) {
                game->penguin_target_cell = -1;
                game->penguin_next_cell = -1;
                game->penguin_move_frame = 0;
                game->penguin_dig_frame = 0;
                game->penguin_pickup_type = 0;
                game->penguin_pickup_frame = 0;
            }
        }
    }
    game->score = 5 * game->counters[1] +
                  12 * game->counters[2] +
                   8 * game->counters[3] +
                  20 * game->counters[4];
    if (game->ticks_remaining > 0) game->ticks_remaining--;
    if (game->ticks_remaining == 0 &&
        game->penguin_target_cell < 0)
        game_point_minigame_begin_result(session, audio_events, now);
}

static void game_update_balloon_minigame(
    GameSession *session, Rich4AudioEventQueue *audio_events, uint32_t now)
{
    static const unsigned char upward_speed[12] = {
        15, 15, 15, 15, 18, 18, 18, 24, 24, 24, 24, 18
    };
    struct PointMinigameState *game = &session->presentation.point_minigame;
    int any_active = 0;
    int i;
    if (game->balloon_slow_ticks > 0)
        game->balloon_slow_ticks--;
    for (i = 0; i < GAME_POINT_MINIGAME_OBJECT_COUNT; i++) {
        struct PointMinigameObject *object = &game->object[i];
        int speed;
        if (!object->active) {
            if (game->ticks_remaining > 0)
                game_point_minigame_spawn_balloon(session, game, audio_events);
            continue;
        }
        any_active = 1;
        if (object->frame > 0) {
            if (++object->frame >= 4)
                memset(object, 0, sizeof(*object));
            continue;
        }
        if (game->balloon_slow_ticks > 0) continue;
        speed = upward_speed[object->type];
        if (game->balloon_speed_mode < 0) speed *= 2;
        else if (game->balloon_speed_mode > 0) speed /= 2;
        object->y -= speed;
        if (object->y < -40)
            memset(object, 0, sizeof(*object));
    }
    if (game->ticks_remaining > 0)
        game->ticks_remaining--;
    if (game->ticks_remaining == 0) {
        any_active = 0;
        for (i = 0; i < GAME_POINT_MINIGAME_OBJECT_COUNT; i++)
            if (game->object[i].active) {
                any_active = 1;
                break;
            }
        if (!any_active)
            game_point_minigame_begin_result(session, audio_events, now);
    }
}

/* 0x413248: collision/drawing precedes falling, mouse movement and spawning. */
static void game_catcher_scene_step(
    GameSession *session, const Rich4PointMinigameCatcherBounds *bounds,
    Rich4AudioEventQueue *audio, int cursor_x)
{
    static const unsigned char speed[5] = {24, 18, 15, 12, 15};
    static const unsigned char source_chunks[5][6] = {
        {0, 1, 2, 3, 5, 6}, {0, 1, 2, 4, 5, 6},
        {7, 8, 9, 10, 11, 0}, {11, 10, 9, 8, 7, 0},
        {12, 13, 14, 15, 17, 18}
    };
    struct PointMinigameState *g = &session->presentation.point_minigame;
    GameCatcherMinigameState *c = &g->catcher;
    int any_active = 0, i;
    memset(c->draw_objects, 0, sizeof(c->draw_objects));
    for (i = 0; i < GAME_POINT_MINIGAME_OBJECT_COUNT; i++) {
        PointMinigameObject *o = &g->object[i];
        int x;
        if (!o->active) continue;
        x = rich4_point_minigame_catcher_draw_x(o);
        if (g->catcher_direction && !g->catcher_bomb_active &&
            bounds != NULL && bounds->valid &&
            x > bounds->left && x < bounds->right &&
            o->y > bounds->top && o->y < bounds->bottom) {
            if (o->type == 4) {
                rich4_point_minigame_emit(audio, RICH4_AUDIO_EVENT_POINT_MINIGAME_STOP_EFFECT, 24);
                rich4_point_minigame_emit(audio, RICH4_AUDIO_EVENT_POINT_MINIGAME_EFFECT, 15);
                g->catcher_direction = 0;
                g->result_tier = 4;
                g->catcher_bomb_active = 1;
                c->explosion_frame = 0;
                c->stage = 1;
            } else {
                g->counters[o->type]++;
            }
            o->active = 0;
        }
        if (o->active) {
            c->draw_objects[i] = *o;
            o->frame = (o->frame + 1) & 7;
            if (o->y < 130) {
                o->velocity_y += 2;
                if (o->velocity_y > 16) o->velocity_y = 16;
                o->y += o->velocity_y;
            } else {
                o->y += speed[o->type];
            }
            if (o->y > 380) {
                if (o->type == 4 && --c->bombs == 0)
                    rich4_point_minigame_emit(audio, RICH4_AUDIO_EVENT_POINT_MINIGAME_STOP_EFFECT, 24);
                o->active = 0;
            }
        }
        if (o->active) any_active = 1;
    }
    c->secondary_draw_frame = g->catcher_secondary_active
        ? g->catcher_secondary_frame : -1;
    c->source_draw_x = g->catcher_source_x;
    c->source_draw_chunk = source_chunks[g->catcher_source_direction][c->source_frame];
    if (c->stage != 2 && !g->catcher_bomb_active) {
        if (!c->mouse_initialized) {
            c->mouse_anchor = cursor_x;
            c->mouse_initialized = 1;
        }
        if (c->mouse_anchor - cursor_x > 8)
            c->mouse_warp = c->mouse_anchor = 0;
        else if (c->mouse_anchor - cursor_x < -8)
            c->mouse_warp = c->mouse_anchor = 639;
        if (abs(g->catcher_x - cursor_x) > 8) {
            g->catcher_direction = g->catcher_x > cursor_x ? 1 : 2;
            g->catcher_x += g->catcher_direction == 1 ? -10 : 10;
            if (++g->catcher_frame == c->move_frames) g->catcher_frame = 0;
        }
    }
    g->score = 10 * g->counters[0] + 5 * g->counters[1] +
                3 * g->counters[2] + g->counters[3];

    if (!g->catcher_secondary_active) {
        if ((g->catcher_source_direction < 2 && g->catcher_source_x > 320) ||
            (g->catcher_source_direction > 3 && g->catcher_source_x < 320)) {
            /* rand_w() is evaluated before the phase test in the original. */
            int trigger = rich4_rng_range_at(&session->state.ai_rng_state, 10) >= 7;
            if (trigger && c->stage == 0) {
                g->catcher_secondary_active = 1;
                g->catcher_secondary_frame = 0;
                c->secondary_x = rich4_rng_range_at(&session->state.ai_rng_state, 140) +
                    (g->catcher_source_x > 320 ? 160 : 360);
            }
        }
    } else {
        if (++g->catcher_secondary_frame == 8 && !g->catcher_bomb_active) {
            rich4_point_minigame_emit(audio, RICH4_AUDIO_EVENT_POINT_MINIGAME_EFFECT, 22);
            rich4_point_minigame_emit(audio, RICH4_AUDIO_EVENT_POINT_MINIGAME_EFFECT, 24);
            game_point_minigame_spawn_catcher_object(session, g, c->secondary_x, 1);
        }
        if (g->catcher_secondary_frame == 12) {
            g->catcher_secondary_active = 0;
            g->catcher_secondary_frame = 0;
        }
    }
    if (c->stage == 1) {
        g->catcher_source_direction = 2;
        c->source_frame = 2;
    } else {
        int direction = g->catcher_source_direction;
        if (direction == 0 || direction == 4) {
            int step = direction == 0 ? 12 : -12;
            if (c->source_frame < 5) {
                if (c->source_frame == c->drop_frame)
                    game_point_minigame_spawn_catcher_object(session, g, g->catcher_source_x, 0);
                c->source_frame++;
                g->catcher_source_x += step;
            } else {
                int beyond_middle = direction == 0 ? g->catcher_source_x > 320
                                                    : g->catcher_source_x < 320;
                int turn = beyond_middle &&
                    rich4_rng_range_at(&session->state.ai_rng_state, 4) == 0;
                if (turn || g->catcher_source_x == (direction == 0 ? 530 : 110)) {
                    g->catcher_source_direction = direction == 0 ? 2 : 3;
                } else {
                    c->drop_frame = rich4_rng_range_at(&session->state.ai_rng_state, 5);
                    g->catcher_source_x += step;
                }
                c->source_frame = 0;
            }
        } else if (direction == 2 || direction == 3) {
            if (++c->source_frame == 5) {
                g->catcher_source_direction = direction == 2 ? 4 : 0;
                c->source_frame = 0;
            }
        }
    }
    /* This is deliberately the scan result BEFORE late dragon spawning. */
    if (!any_active && c->stage == 1) c->stage = 2;
}

static void game_update_catcher_minigame(
    GameSession *session, const Rich4PointMinigameCatcherBounds *bounds,
    Rich4AudioEventQueue *audio, uint32_t now, int cursor_x)
{
    struct PointMinigameState *g = &session->presentation.point_minigame;
    if (g->ticks_remaining > 0 && --g->ticks_remaining == 0)
        g->catcher.stage = 1;
    game_catcher_scene_step(session, bounds, audio, cursor_x);
    if (g->catcher.stage == 2) {
        game_point_minigame_begin_result(session, audio, now);
        /* 0x41473B redraws (and advances) the scene once for the result. */
        game_catcher_scene_step(session, NULL, audio, cursor_x);
        g->catcher_bomb_active = 0;
    } else if (g->catcher_bomb_active) {
        /* Data[526] has eight frames; callback 0x414FCD decodes one/tick. */
        if (++g->catcher.explosion_frame == 8) {
            g->catcher_bomb_active = 0;
            g->catcher.stage = 2;
        }
    }
}

int rich4_point_minigame_update(
    GameSession *session, int intro_frames, int intro_delay_ms,
    const Rich4PointMinigameCatcherBounds *catcher_bounds,
    uint32_t now, int cursor_x, Rich4AudioEventQueue *audio_events)
{
    struct PointMinigameState *game;
    if (session == NULL || !session->presentation.point_minigame.active) return 0;
    game = &session->presentation.point_minigame;
    if (game->phase == RICH4_POINT_MINIGAME_READY) {
        if (!game->catcher.scene_initialized) {
            game_catcher_scene_step(session, catcher_bounds, audio_events, cursor_x);
            game->catcher.scene_initialized = 1;
        }
        if ((int32_t)(now - game->next_tick) >= 0) {
            game->next_tick = now + 50;
            if (--game->catcher.wait_ticks == 0) {
                game->phase = RICH4_POINT_MINIGAME_INTRO;
                rich4_animation_clock_reset(&session->presentation.animation_clock[RICH4_ANIMATION_POINT_INTRO]);
                (void)rich4_animation_clock_step(
                    &session->presentation.animation_clock[RICH4_ANIMATION_POINT_INTRO],
                    now, intro_delay_ms > 0 ? intro_delay_ms : 114);
            }
        }
        return 1;
    }
    if (game->phase == RICH4_POINT_MINIGAME_INTRO) {
        int frames = intro_frames;
        int delay = intro_delay_ms;
        if (delay <= 0) delay = 114;
        if (rich4_animation_clock_step(
                &session->presentation.animation_clock[RICH4_ANIMATION_POINT_INTRO],
                now, delay)) game->animation_frame++;
        game->next_tick = session->presentation.animation_clock[RICH4_ANIMATION_POINT_INTRO].tick;
        if (frames <= 0 || game->animation_frame >= frames) {
            game->animation_frame = frames > 0 ? frames - 1 : 0;
            game->phase = RICH4_POINT_MINIGAME_PLAY;
            game->next_tick = now + (game->action == 8 ? 50u : 0u);
            if (game->action == 8) {
                game->catcher.mouse_anchor = cursor_x;
                game->catcher.mouse_initialized = 1;
            }
        }
        return 1;
    }
    if (game->phase == RICH4_POINT_MINIGAME_RESULT) {
        int automatic = game->automatic;
        int player = game->player;
        game->animation_frame =
            (int)((now - game->next_tick) / 100u);
        if ((int32_t)(now - game->finish_until) >= 0) {
            game_finish_point_minigame(session, audio_events);
            if (!game->active && automatic && player >= 0 &&
                player < session->state.player_count) {
                int variant = automatic >=
                        RICH4_POINT_MINIGAME_AUTOMATIC_RESOLVED_FIRST_SPEECH
                    ? automatic - RICH4_POINT_MINIGAME_AUTOMATIC_RESOLVED_FIRST_SPEECH
                    : (int)(rich4_rng_next_at(&session->state.ai_rng_state) & 1u);
                const Rich4CharacterSpeech *speech =
                    rich4_character_reward_speech(
                        session->state.players[player].character, 1,
                        variant);
                game_start_speech_entry(
                    session, player, speech, 0,
                    GAME_PROPERTY_PRESENT_NONE, now);
            }
        }
        /* The speech begins on this frame; keep ordinary turn dispatch
         * paused until its presentation handler takes over next frame. */
        return session->presentation.point_minigame.active ||
               session->presentation.speech.active;
    }
    {
        uint32_t delay = game->action == 8 ? 50u : 100u;
        if ((int32_t)(now - game->next_tick) < 0) return 1;
        game->next_tick += delay;
        if ((int32_t)(now - game->next_tick) >= 0)
            game->next_tick = now + delay;
    }
    if (game->action == 6)
        game_update_penguin_minigame(session, audio_events, now);
    else if (game->action == 7)
        game_update_balloon_minigame(session, audio_events, now);
    else
        game_update_catcher_minigame(
            session, catcher_bounds, audio_events, now, cursor_x);
    return session->presentation.point_minigame.active;
}

void rich4_point_minigame_mark_music_started(GameSession *session)
{
    if (session != NULL)
        session->presentation.point_minigame.music_started = 1;
}

int rich4_point_minigame_take_cursor_warp(GameSession *session)
{
    int x;
    if (session == NULL || !session->presentation.point_minigame.active ||
        session->presentation.point_minigame.action != 8 ||
        session->presentation.point_minigame.automatic) return -1;
    x = session->presentation.point_minigame.catcher.mouse_warp;
    session->presentation.point_minigame.catcher.mouse_warp = -1;
    return x;
}

void rich4_point_minigame_pause(GameSession *session, uint32_t now)
{
    if (session == NULL) return;
    session->presentation.point_minigame.next_tick = now + 50;
    session->presentation.animation_clock[RICH4_ANIMATION_POINT_INTRO].tick = now;
}
