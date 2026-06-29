#ifndef RICH4_VIEW_CONSTANTS_H
#define RICH4_VIEW_CONSTANTS_H

#define SCREEN_W 640
#define SCREEN_H 480

/* sub_40829D adds the sub-tile projection to the 440x440 map viewport centre. */
#define GAME_MAIN_MAP_ORIGIN_X 220
#define GAME_MAIN_MAP_ORIGIN_Y 260
#define GAME_MAIN_MAP_LEFT 0
#define GAME_MAIN_MAP_TOP 40
/* The right/bottom values are exclusive geometry bounds.  The original
 * selector compares against x >= 440 and y == 479. */
#define GAME_MAIN_MAP_RIGHT 440
#define GAME_MAIN_MAP_BOTTOM 480
#define GAME_MAIN_MAP_TILE_W 51
#define GAME_MAIN_MAP_TILE_H 49
#define GAME_MAIN_MAP_UX 34
#define GAME_MAIN_MAP_UY -11
#define GAME_MAIN_MAP_VX 14
#define GAME_MAIN_MAP_VY 25
#define GAME_MINIMAP_X 440
#define GAME_MINIMAP_Y 280
#define GAME_MINIMAP_W 200
#define GAME_MINIMAP_H 200
#define GAME_MINIMAP_ROTATE_Y (GAME_MINIMAP_Y + 3)
#define GAME_MINIMAP_ROTATE_LEFT_X (GAME_MINIMAP_X + 3)
#define GAME_MINIMAP_ROTATE_RIGHT_X (GAME_MINIMAP_X + 28)
#define GAME_MINIMAP_ROTATE_W 25
#define GAME_MINIMAP_ROTATE_H 26
#define GAME_ORIGINAL_MINIMAP_SCALE 5696
#define GAME_WORLD_MAX_X (72 * 32 - 1)
#define GAME_WORLD_MAX_Y (72 * 32 - 1)

/* Full-screen overview overlay (distinct from the always-on GAME_MINIMAP_*
 * panel above). Shared by the event loop's open/close lifecycle and the
 * game_minimap_render.c panel/marker drawing. */
#define GAME_SMALL_MAP_LEFT 20
#define GAME_SMALL_MAP_TOP 60
#define GAME_SMALL_MAP_WIDTH 400
#define GAME_SMALL_MAP_HEIGHT 400
#define GAME_ORIGINAL_SMALL_MAP_SCALE 11392
#define GAME_SMALL_MAP_LAND_MARKER_FIRST 26
#define GAME_SMALL_MAP_FACILITY_MARKER_FIRST 28

#define GAME_DICE_RESULT_HOLD_MS 500
#define GAME_TIMER_FRAME_MS 20
#define GAME_DICE_POPUP_BASE_X 136
#define GAME_DICE_POPUP_BASE_Y 48
#define GAME_DICE_FACE_OFFSET_X 85
#define GAME_DICE_FACE_OFFSET_Y 145
#define GAME_MOVE_FRAME_MS 20
#define GAME_INTRO_DEFAULT_FRAME_MS 55
#define GAME_INTRO_AVI_NAME_MAX 16
#define GAME_LANDING_FRAME_MS 55
#define GAME_LANDING_FRAMES 24
#define GAME_DICE_STATUS_X 180
#define GAME_DICE_STATUS_Y 120
#define GAME_DICE_STATUS_W 72
#define GAME_DICE_STATUS_H 67

#define RICH4_FONT_DARK 0x0842
#define RICH4_FONT_LIGHT 0xf79e

#endif
