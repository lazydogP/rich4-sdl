#ifndef RICH4_APP_HOTEL_FLOW_H
#define RICH4_APP_HOTEL_FLOW_H

#include "app/game_session.h"

/* 0x40D5A5 / 0x40C05C: entry uses the existing traffic sprite lane. */
int rich4_hotel_capture_entry(GameSession *session, const GameEvent *event);
int rich4_hotel_update_entry(GameSession *session, const MapData *map,
                              uint32_t now, int frame_ms);
int rich4_hotel_anchor(const GameState *state, const MapData *map, int player,
                        int *x, int *y);

#endif
