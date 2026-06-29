#ifndef RICH4_APP_SALE_INTERACTION_FLOW_H
#define RICH4_APP_SALE_INTERACTION_FLOW_H

#include "app/app_command.h"
#include "app/game_session.h"
#include "app/stock_interaction_flow.h"

enum {
    RICH4_SALE_POINTER_BUTTON_LEFT = 1,
    RICH4_SALE_POINTER_BUTTON_RIGHT = 3
};

typedef enum Rich4AppSaleAudioCue {
    RICH4_APP_SALE_AUDIO_NONE = 0,
    RICH4_APP_SALE_AUDIO_SELECT,
    RICH4_APP_SALE_AUDIO_CANCEL,
    RICH4_APP_SALE_AUDIO_OK
} Rich4AppSaleAudioCue;

typedef struct Rich4AppSaleInteractionResult {
    int command_ready;
    GameCommand command;
    Rich4AppSaleAudioCue audio_cue;
} Rich4AppSaleInteractionResult;

int rich4_app_sale_interaction_is_active(const GameSession *session);
int rich4_app_sale_item_count(
    const GameSession *session, Rich4SaleInteractionCategory category);
int rich4_app_sale_item_at(
    const GameSession *session, Rich4SaleInteractionCategory category,
    int ordinal, GameDomainRef *out_item, int *out_maximum);
int rich4_app_sale_browse_count(const GameSession *session);
int rich4_app_sale_browse_listing_at(
    const GameSession *session, int ordinal, int *out_seller, int *out_slot);
void rich4_app_sale_interaction_open(GameSession *session);
void rich4_app_sale_interaction_cancel(GameSession *session);
int rich4_app_sale_interaction_entry_value(const GameSession *session);
int rich4_app_sale_interaction_entry_maximum(const GameSession *session);
int rich4_app_sale_interaction_entry_reference_price(
    const GameSession *session);
int rich4_app_sale_interaction_entry_is_quantity(
    const GameSession *session);
void rich4_app_sale_interaction_mouse_down(
    GameSession *session, const Rich4StockQuantityMask *mask, int x, int y,
    int stock_panel_width, int property_panel_width,
    int confirm_dialog_width, int confirm_dialog_height);
void rich4_app_sale_interaction_mouse_motion(
    GameSession *session, const Rich4StockQuantityMask *mask, int x, int y,
    int mouse_left_down, int stock_panel_width, int property_panel_width,
    int confirm_dialog_width, int confirm_dialog_height);
void rich4_app_sale_interaction_mouse_release(
    GameSession *session, const Rich4StockQuantityMask *mask, int button,
    int x, int y, int stock_panel_width, int property_panel_width,
    int confirm_dialog_width, int confirm_dialog_height,
    Rich4AppSaleInteractionResult *out_result);
void rich4_app_sale_interaction_key_down(
    GameSession *session, Rich4AppKey key,
    Rich4AppSaleInteractionResult *out_result);
void rich4_app_sale_interaction_command_applied(
    GameSession *session, int succeeded,
    Rich4AppSaleInteractionResult *result);

#endif
