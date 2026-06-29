#ifndef RICH4_CORE_GAME_TYPES_H
#define RICH4_CORE_GAME_TYPES_H

#include <stdint.h>

#include "core/game_constants.h"

typedef struct GamePlayerState {
    int character;
    /* Original character table +0x15, retained for magic-house targeting. */
    int sex;
    int is_ai;
    int node;
    int cash;
    int bank;
    int loan;
    /* Original player record +44: repayment deadline used by bank AI. */
    int loan_due_date;
    int special_finance;
    /* Original player record +0x30: 16-bit 点券 balance. */
    uint16_t points;
    int relation_balance[GAME_PLAYER_RELATION_BALANCE_COUNT];
    /* Original 0x499120 layout: 15 one-byte card IDs per player, 0 is empty. */
    uint8_t card_inventory[GAME_PLAYER_CARD_SLOTS];
    /* Original 0x49915c layout: 4 * 15 one-byte quantities; slots 13/14 reserved. */
    uint8_t tool_inventory[GAME_PLAYER_TOOL_SLOTS];
    int traffic_method;
    int ndices;
    /* sub_4470F8 / byte 0x475DD8: one-shot result selected by 遥控骰子. */
    int remote_dice_value;
    int saved_traffic_method;
    int saved_ndices;
    /* Direction toggle written by 转向卡. The route adapter consumes this
     * when choosing previous/next edges. */
    int route_reversed;
    /* Original +100/+101 used by the engineering vehicle, not status restore. */
    int engineering_saved_traffic_method;
    int engineering_saved_ndices;
    int attached_timed_bomb_slot;
    int next_node;
    int previous_node;
    int play_state;
    int delayed_status_block;
    int days_disappearing;
    int days_in_prison;
    int days_in_hospital;
    int days_sleeping;
    int days_sleep_walking;
    int days_stopping;
    int days_tortoise_walking;
    int days_rejected_by_bank;
    int days_bank_no_loans;
    int god_status_days;
    int days_assurance;
    int god_info;
    /* Host-side resolved dynamic-object type; original +63 stores only the slot. */
    int god_type;
    /* Original god attachment modifiers at player-record +68/+70/+72.
     * They are kept as signed ints while preserving the original table
     * values; attach/detach applies the corresponding delta transactionally. */
    int god_income_modifier;
    int god_investment_modifier;
    int god_land_modifier;
    int linked_player;
    int confinement_actor_type;
    /* Original +0x42 monthly bad-day byte; month-end resets it. */
    int total_confinement_days;
    /* Original player record +22: bit 0 cards, bit 1 tools. */
    int ai_action_flags;
    /* Original player record +23: 0=乖宝宝, 1=普通人, 2=大老奸. */
    int ai_personality;
    /* Original character defaults +24/+25/+26 retained for AI policy. */
    int ai_trait;
    int ai_cash_ratio;
    int ai_stock_ratio;
    /* 0x496BC4/+0x5c and 0x496BC8/+0x60. */
    int monthly_loss;
    int monthly_income;
} GamePlayerState;

typedef struct GameStockInfo {
    uint16_t f4;
    uint8_t f6;
    uint8_t f7;
    uint16_t f8;
    uint16_t f10;
    float f12;
    float f16;
    float f20;
    float f24;
    float f28;
    uint32_t f32;
} GameStockInfo;

typedef struct GameCharacterTemplate {
    uint32_t color;
    int sex;
    int f22;
    int f23;
    int f24;
    int init_cash_ratio;
    int f26;
} GameCharacterTemplate;

#endif
