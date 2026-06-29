#include "app/point_minigame_flow.h"
#include "app/audio_event.h"
#include "core/rng.h"

#include <string.h>

void rich4_point_minigame_cell_xy(
    int cell, int *out_x, int *out_y)
{
    int column = cell % 9;
    int row = cell / 9;
    if (out_x != NULL) *out_x = 48 * (row + column) - 64;
    if (out_y != NULL) *out_y = 225 + 24 * (row - column);
}

int rich4_point_minigame_click(
    GameSession *session, int mask_cell,
    int x, int y, uint32_t now, Rich4AudioEventQueue *audio_events)
{
    struct PointMinigameState *game;
    int i;
    if (session == NULL || !session->presentation.point_minigame.active)
        return 0;
    (void)now;
    game = &session->presentation.point_minigame;
    if (game->phase != RICH4_POINT_MINIGAME_PLAY)
        return 1;
    if (game->action == 6) {
        int cell;
        if (game->penguin_target_cell >= 0 ||
            game->ticks_remaining <= 0)
            return 1;
        cell = mask_cell;
        if (cell >= 0 && cell != game->penguin_player_cell) {
            game->penguin_target_cell = cell;
            game->penguin_next_cell =
                rich4_point_minigame_penguin_next_cell(
                    game->penguin_player_cell, cell);
            game->penguin_move_frame = 0;
            game->penguin_dig_frame = 0;
            game->penguin_pickup_frame = 0;
        }
        return 1;
    }
    if (game->action != 7) return 1;
    for (i = 0; i < GAME_POINT_MINIGAME_OBJECT_COUNT; i++) {
        struct PointMinigameObject *object = &game->object[i];
        int half_width;
        int half_height;
        int type;
        if (!object->active || object->frame > 0) continue;
        half_width = object->type < 6 ? 22 : 18;
        half_height = object->type < 6 ? 30 : 26;
        if (x < object->x - half_width ||
            x > object->x + half_width ||
            y < object->y - half_height ||
            y > object->y + half_height) {
            rich4_audio_event_emit(audio_events, RICH4_AUDIO_EVENT_POINT_MINIGAME_EFFECT, 1, 20);
            continue;
        }
        rich4_audio_event_emit(audio_events, RICH4_AUDIO_EVENT_POINT_MINIGAME_EFFECT, 1, 21);
        type = object->type;
        if (type < 9) {
            game->score += type + 1;
        } else if (type == 9) {
            game->score *= 2;
        } else if (type == 10) {
            game->score >>= 1;
        } else {
            switch (rich4_rng_range_at(&session->state.ai_rng_state, 6)) {
            case 0:
                game->ticks_remaining = 0;
                break;
            case 1:
                game->balloon_slow_ticks = 20;
                break;
            case 2:
                game->balloon_speed_mode = -1;
                break;
            case 3:
                game->balloon_speed_mode = 1;
                break;
            case 4:
                game->score = 0;
                break;
            default:
                game->score *= 2;
                break;
            }
        }
        if (game->score > 999) game->score = 999;
        object->frame = 1;
        return 1;
    }
    return 1;
}


