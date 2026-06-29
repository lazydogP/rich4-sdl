#ifndef RICH4_APP_DICE_FLOW_H
#define RICH4_APP_DICE_FLOW_H

#include <stddef.h>
#include <stdint.h>

#include "app/app_command.h"
#include "app/game_session.h"
#include "data/map_data.h"

/* App-owned dice turn flow. Platform supplies resource-derived timing and
 * the immutable route map used when the popup hands off to movement. */
void rich4_dice_begin_roll(GameSession *session, uint32_t now);
int rich4_dice_update(GameSession *session, const MapData *map,
                      int action_frames, int action_delay_ms,
                      int graph_frames, int graph_delay_ms,
                      uint32_t now);

void rich4_dice_show_action_panel(GameSession *session, uint32_t now,
                                  int action_hint, int input_refresh);
void rich4_dice_clear_action_panel_flags(GameSession *session);
void rich4_dice_release_status_drag(GameSession *session);
int rich4_dice_consume_cache_reset(GameSession *session);
int rich4_dice_status_mouse_down(
    GameSession *session, const unsigned char *mask_data, size_t mask_size,
    int mouse_x, int mouse_y, uint32_t now);
void rich4_dice_update_status_drag(GameSession *session,
                                   int mouse_x, int mouse_y);
int rich4_dice_next_count(const GameSession *session);
int rich4_dice_shortcut_key_down(GameSession *session, Rich4AppKey key,
                                 int game_active, uint32_t now);
void rich4_dice_update_status_blink(GameSession *session, uint32_t now);

#endif
