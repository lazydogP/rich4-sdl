#ifndef RICH4_APP_MAP_EDGE_SCROLL_STATE_H
#define RICH4_APP_MAP_EDGE_SCROLL_STATE_H

#include <stdint.h>

/* Transient state for the original selector's 50 ms edge-scroll timer.
 * direction is 1=up, 2=left, 3=down, 4=right; speed is the original
 * camera-step scalar, not a screen-pixel velocity. */
typedef struct Rich4MapEdgeScrollState {
    int direction;
    int speed;
    uint32_t tick;
} Rich4MapEdgeScrollState;

#endif
