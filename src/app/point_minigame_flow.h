#ifndef RICH4_APP_POINT_MINIGAME_FLOW_H
#define RICH4_APP_POINT_MINIGAME_FLOW_H

#include <stdint.h>

#include "app/audio_event.h"
#include "app/game_session.h"
#include "core/game_event.h"

/* Platform supplies only the current catcher sprite bounds used for hit
 * testing; all minigame state transitions remain app-owned. */
typedef struct Rich4PointMinigameCatcherBounds {
    int valid;
    int left;
    int top;
    int right;
    int bottom;
} Rich4PointMinigameCatcherBounds;

int rich4_point_minigame_cell_valid(int column, int row);
int rich4_point_minigame_penguin_next_cell(int from, int to);
int rich4_point_minigame_catcher_draw_x(
    const struct PointMinigameObject *object);
void rich4_point_minigame_cell_xy(int cell, int *out_x, int *out_y);

void rich4_point_minigame_begin(
    GameSession *session, const GameEvent *event, uint32_t now,
    int animation_enabled, Rich4AudioEventQueue *audio_events);
int rich4_point_minigame_update(
    GameSession *session, int intro_frames, int intro_delay_ms,
    const Rich4PointMinigameCatcherBounds *catcher_bounds,
    uint32_t now, int cursor_x, Rich4AudioEventQueue *audio_events);
int rich4_point_minigame_click(
    GameSession *session, int mask_cell, int x, int y, uint32_t now,
    Rich4AudioEventQueue *audio_events);
void rich4_point_minigame_mark_music_started(GameSession *session);
int rich4_point_minigame_take_cursor_warp(GameSession *session);
void rich4_point_minigame_pause(GameSession *session, uint32_t now);

#endif
