#ifndef RICH4_PLATFORM_SDL1_GAME_POINT_MINIGAME_VIEW_H
#define RICH4_PLATFORM_SDL1_GAME_POINT_MINIGAME_VIEW_H

#include <SDL/SDL.h>

#include "app/game_session.h"
#include "core/game_constants.h"

typedef struct GamePointMinigameObjectView {
    int active;
    int x;
    int y;
    int type;
    int frame;
    int velocity_x;
    int velocity_y;
} GamePointMinigameObjectView;

typedef struct GamePointMinigameRenderState {
    int active;
    int automatic;
    int music_started;
    int action;
    int player;
    int phase;
    int ticks_remaining;
    int score;
    int result_tier;
    int animation_frame;
    int animation_delay;
    Uint32 next_tick;
    Uint32 finish_until;
    int counters[5];
    int penguin_treasure[81];
    int penguin_player_cell;
    int penguin_target_cell;
    int penguin_next_cell;
    int penguin_move_frame;
    int penguin_dig_frame;
    int penguin_pickup_type;
    int penguin_pickup_frame;
    int balloon_slow_ticks;
    int balloon_speed_mode;
    int catcher_x;
    int catcher_direction;
    int catcher_frame;
    int catcher_source_x;
    int catcher_source_direction;
    int catcher_secondary_active;
    int catcher_secondary_frame;
    int catcher_bomb_active;
    int catcher_source_chunk;
    int catcher_secondary_x;
    int catcher_explosion_frame;
    GamePointMinigameObjectView object[GAME_POINT_MINIGAME_OBJECT_COUNT];
} GamePointMinigameRenderState;

typedef struct GamePointMinigameRenderView {
    GamePointMinigameRenderState game;
    int player_character;
} GamePointMinigameRenderView;

int build_point_minigame_render_view(const GameSession *session,
                                     GamePointMinigameRenderView *out_view);

#endif
