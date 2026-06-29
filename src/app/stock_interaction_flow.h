#ifndef RICH4_APP_STOCK_INTERACTION_FLOW_H
#define RICH4_APP_STOCK_INTERACTION_FLOW_H

#include <stddef.h>

#include "app/app_command.h"
#include "app/game_session.h"

/* App-owned stock-panel interaction. SDL supplies only normalized pointer
 * buttons, keys, render resources and the command-application bridge. */
enum {
    RICH4_APP_POINTER_BUTTON_LEFT = 1,
    RICH4_APP_POINTER_BUTTON_RIGHT = 3
};

enum {
    RICH4_STOCK_QUANTITY_CONTROL_NONE = 0,
    RICH4_STOCK_QUANTITY_CONTROL_DRAG = 1,
    RICH4_STOCK_QUANTITY_CONTROL_MAX = 2,
    RICH4_STOCK_QUANTITY_CONTROL_ENTER = 3,
    RICH4_STOCK_QUANTITY_CONTROL_SLIDER = 16
};

typedef struct Rich4StockQuantityMask {
    const unsigned char *data;
    size_t size;
    int available;
} Rich4StockQuantityMask;

typedef enum Rich4AppStockAudioCue {
    RICH4_APP_STOCK_AUDIO_NONE = 0,
    RICH4_APP_STOCK_AUDIO_SELECT,
    RICH4_APP_STOCK_AUDIO_CANCEL,
    RICH4_APP_STOCK_AUDIO_SLIDER,
    RICH4_APP_STOCK_AUDIO_KEY,
    RICH4_APP_STOCK_AUDIO_OK
} Rich4AppStockAudioCue;

typedef struct Rich4AppStockInteractionResult {
    int command_ready;
    GameCommand command;
    Rich4AppStockAudioCue audio_cue;
} Rich4AppStockInteractionResult;

int rich4_app_stock_interaction_is_active(const GameSession *session);
int rich4_app_stock_interaction_hit(int x, int y);
int rich4_app_stock_quantity_control_at(int x, int y);
int rich4_app_stock_quantity_control_at_mask(
    const Rich4StockQuantityMask *mask, int x, int y);
int rich4_app_stock_quantity_bar_width(int quantity, int maximum);
int rich4_app_stock_quantity_from_bar(int maximum, int pointer_x);

void rich4_app_stock_interaction_open(GameSession *session);
void rich4_app_stock_interaction_close(GameSession *session);
void rich4_app_stock_interaction_cancel(GameSession *session);
void rich4_app_stock_interaction_mouse_down(
    GameSession *session, int x, int y);
void rich4_app_stock_interaction_mouse_down_with_mask(
    GameSession *session, const Rich4StockQuantityMask *mask, int x, int y);
void rich4_app_stock_interaction_mouse_release(
    GameSession *session, int button, int x, int y,
    Rich4AppStockInteractionResult *out_result);
void rich4_app_stock_interaction_mouse_release_with_mask(
    GameSession *session, const Rich4StockQuantityMask *mask, int button,
    int x, int y, Rich4AppStockInteractionResult *out_result);
void rich4_app_stock_interaction_mouse_motion(
    GameSession *session, const Rich4StockQuantityMask *mask, int x, int y,
    Rich4AppStockInteractionResult *out_result);
void rich4_app_stock_interaction_key_down(
    GameSession *session, Rich4AppKey key,
    Rich4AppStockInteractionResult *out_result);
int rich4_app_stock_interaction_command_applied(
    GameSession *session, int succeeded);
size_t rich4_app_stock_interaction_cursor_chunk_index(
    const GameSession *session, size_t default_chunk);

#endif
