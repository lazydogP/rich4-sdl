#ifndef RICH4_APP_GAME_PRESENTATION_FEATURE_STATES_H
#define RICH4_APP_GAME_PRESENTATION_FEATURE_STATES_H

#include <stdint.h>

#include "core/game_command.h"
#include "core/game_constants.h"
#include "core/game_domain.h"
#include "app/game_presentation_interactive_states.h"
#include "data/character_speech.h"
#include "core/periodic_finance.h"
#include "core/game_event.h"

typedef struct Rich4CharacterSpeech Rich4CharacterSpeech;

/* Original blocking map notices, distinct from interactive place panels. */
typedef struct GameSpecialPlaceNoticePresentationState {
    GameEventQueue queue;
    uint32_t until;
    int speech_pending;
} GameSpecialPlaceNoticePresentationState;

enum {
    RICH4_INSURANCE_NOTICE_CAPACITY = 8,
    GAME_INSURANCE_NOTICE_FEE = 1,
    GAME_INSURANCE_NOTICE_CLAIM = 2,
    /* 0x44BAC1 waits for the notice before 0x44BAD8 pays the claim. */
    GAME_INSURANCE_NOTICE_PENDING_CONFINEMENT_CLAIM = 3,
    GAME_INSURANCE_NOTICE_PENDING_HOTEL_CLAIM = 4,
    GAME_INSURANCE_NOTICE_PENDING_CLAIM = 5,
    /* Fortune callbacks must finish their payer/day speech before 0x44BA63. */
    GAME_INSURANCE_NOTICE_WAIT_SPECIAL = 6
};

typedef struct GameSpecialRewardPresentationState {
    int active;
    int action;
    int player;
    /* Zero-based card ID; road gift action 15 stores a tool ID here. */
    int card;
    int phase;
    int frame;
    int sound_played;
    uint32_t tick;
    uint32_t until;
} GameSpecialRewardPresentationState;

enum {
    RICH4_GOD_MESSAGE_HIDDEN = 0,
    RICH4_GOD_MESSAGE_MAIN = 1,
    RICH4_GOD_MESSAGE_RESULT = 2
};

/* 0x43F23E/0x43EF3E locals, kept across SDL frames. Panel[67] has twenty
 * half-digit frames; digits stop from units towards thousands. */
typedef struct GameGodMoneyPresentationState {
    int active;
    int phase;
    int mode;
    int automatic;
    int digits[4];
    int ticks;
    int slowdown_digit;
    int slowdown_delay;
    int slowdown_ticks;
    int amount;
    int settlement_player;
    uint32_t next_tick;
} GameGodMoneyPresentationState;

/* 0x40EAD7/0x41D546 god-acquisition overlay.  The app owns only primitive
 * timing and decoded event identity; SDL borrows the corresponding Data.MKF
 * animation and draws the original Big5 message. */
typedef struct GameGodAcquisitionPresentationState {
    int active;
    int player;
    int god_type;
    int object_slot;
    int first_acquisition;
    /* Type 3/4 and 7 use the original card-name follow-up text; type 8
     * reports the number removed.  -1 means no card result was produced. */
    int result_card_1;
    int result_card_2;
    int result_lost_count;
    int sound_pending;
    int frame;
    int frame_count;
    int frame_delay_ms;
    /* 0 while the FLC is running, 1 during 0x40E2A2, 2 during the
     * type-specific 0x440CAC follow-up. */
    int message_visible;
    uint32_t started;
    uint32_t frame_tick;
    uint32_t animation_until;
    uint32_t until;
    /* Transient modal fields: saving is blocked until settlement completes. */
    int death_pending;
    /* 1: not started, 2: waiting for arrival speech, 0: complete. */
    int arrival_speech_phase;
    int money_pending;
    int waiting_for_detach;
    GameGodMoneyPresentationState money;
} GameGodAcquisitionPresentationState;

enum {
    RICH4_SPECIAL_PRESENT_PAGE = 0,
    RICH4_SPECIAL_PRESENT_HOLD,
    RICH4_SPECIAL_PRESENT_CONSEQUENCE,
    RICH4_SPECIAL_PRESENT_REACTION,
    RICH4_SPECIAL_PRESENT_CONFINEMENT_ANIMATION,
    RICH4_SPECIAL_PRESENT_POST_HOLD,
    RICH4_SPECIAL_PRESENT_FORTUNE_ANIMATION,
    RICH4_SPECIAL_PRESENT_REACTION_DELAY,
    /* Fortune 5 phase-1 text: one human-visible victim at a time. */
    RICH4_SPECIAL_PRESENT_FORTUNE_CARD_TARGET,
    RICH4_SPECIAL_PRESENT_PASSIVE_CARD,
    RICH4_SPECIAL_PRESENT_FORTUNE_REDIRECT_TARGET,
    RICH4_SPECIAL_PRESENT_GOD_NOTICE,
    RICH4_SPECIAL_PRESENT_REDIRECT_NOTICE,
    RICH4_SPECIAL_PRESENT_PASSIVE_ACTOR_SPEECH,
    RICH4_SPECIAL_PRESENT_PASSIVE_VICTIM_SPEECH
};
#define RICH4_SPECIAL_PRESENTATION_PLAYER_SHIFT 16

typedef struct GameBankRejectionPresentationState {
    int active;
    int days;
    uint32_t until;
} GameBankRejectionPresentationState;

enum {
    RICH4_BANKRUPTCY_PHASE_ANIMATION = 0,
    RICH4_BANKRUPTCY_PHASE_HOLD = 1,
    RICH4_BANKRUPTCY_PHASE_SPEECH = 2
};

/* 0x40CD87 plays Data.MKF[555] after the ledger has already cleared the
 * player.  Auction requests are released only after this animation and its
 * fixed 2000 ms hold, matching the blocking original call sequence. */
typedef struct GameBankruptcyPresentationState {
    int active;
    int player;
    int phase;
    int frame;
    int frame_count;
    int frame_delay_ms;
    int awaiting_auction;
    /* Saved before ledger cleanup so the original human/AI branch can be
     * selected after the modal boundary. */
    int original_play_state;
    int previous_current_player;
    int post_speech_lifecycle;
    /* Transient continuation of options 0x411AE0, after 0x40CD87 returns. */
    int surrender_followup;
    uint32_t started;
    uint32_t frame_tick;
    uint32_t animation_until;
    uint32_t until;
} GameBankruptcyPresentationState;

typedef struct GameCompanyIssuePresentationState {
    int active;
    int player;
    int company;
    int stock;
    int phase;
    int price;
    int maximum;
    int quantity;
    uint32_t notice_until; /* phase 2: 0x41D2AA, 1500 ms */
    /* Transient sub_452C02 keypad locals. */
    int entry_x, entry_y, entry_pressed, entry_dragging, entry_dx, entry_dy;
} GameCompanyIssuePresentationState;

typedef struct GameFacilityRoulettePresentationState {
    int active;
    int player;
    int mode;
    int phase;
    int slot;
    /* 0x44090E draws Panel[68+mode] chunk 2 before the first
     * 0x24-ms 0x43F127 tick.  Keep that one-frame placeholder separate
     * from the random slot selected by 0x43F7C6. */
    int initial_frame_pending;
    int steps;
    int delay;
    int outcome;
    int automatic;
    /* The original 0x43F7C6 locals: outer-loop ticks and the
     * stage/cycle/last-tick trio used by its deceleration state. */
    int tick_count;
    int deceleration_stage;
    int deceleration_cycles;
    int deceleration_last_tick;
    GameDomainRef subject;
    uint32_t next_tick;
    uint32_t finish_until;
} GameFacilityRoulettePresentationState;

typedef struct GameBankSettlementPresentationState {
    int active;
    unsigned int player_mask;
    unsigned int loan_mask;
    int interest[GAME_MAX_PLAYERS];
    int balance[GAME_MAX_PLAYERS];
    int frame;
    uint32_t tick;
    uint32_t until;
    /* Snapshot before 0x439EDE clears the monthly statistics. */
    int monthly_loss[GAME_MAX_PLAYERS];
    int monthly_income[GAME_MAX_PLAYERS];
    int monthly_bad_days[GAME_MAX_PLAYERS];
    int assets[GAME_MAX_PLAYERS];
    int cash[GAME_MAX_PLAYERS];
    int unlucky_player;
    int richest_player;
    int stage;
    int stage_ticks;
    int display_stage;
    int display_speech_id;
    int skip_requested;
    int speech_id;
    int speech_pending;
    int animation_frame;
    uint32_t animation_tick;
    uint32_t stage_started;
    int effect_pending;
    /* 0x48C42C/0x48C42B: monthly banker's idle eyes and speech mouth. */
    unsigned int eye_state;
    int eye_chunk;
    int mouth_ticks;
    int mouth_chunk;
    int face_mode;
} GameBankSettlementPresentationState;

/* 0x42BA97 -> 0x43010C: a single owner for the 15th-day modal chain. */
typedef struct GamePeriodicFinancePresentationState {
    int active;
    int phase;
    int ticks;
    int close_requested;
    int music_started;
    int speech_id;
    int speech_pending;
    int number;
    int winner;
    int pool;
    int animation_frame;
    int left_expression;
    int right_expression;
    int effect_pending;
    uint32_t tick;
    uint32_t animation_tick;
    uint32_t stage_started;
    unsigned int host_state;
    int mouth_ticks;
    int mouth_chunk;
    int credit_player;
    Rich4CompanyDividend dividend;
} GamePeriodicFinancePresentationState;

typedef struct GameBankLoanDuePresentationState {
    int active;
    int player;
    int days;
    int principal;
    int forced;
    int payment;
    uint32_t until;
} GameBankLoanDuePresentationState;

typedef struct GameConfinementStatusPresentationState {
    int active;
    int player;
    int slot;
    int days;
    uint32_t until;
} GameConfinementStatusPresentationState;

typedef struct GameConfinementVisitPresentationState {
    int active;
    int kind;
    int phase;
    int hover;
    int selected;
    int confirm_hover;
    uint32_t until;
    Rich4CharacterSpeechSnapshot speech_snapshot;
    Rich4CharacterSpeech restored_speech;
    const Rich4CharacterSpeech *speech;
    int speech_active;
    int voice_started;
    int speech_special_actor;
    uint32_t speech_tick;
    uint32_t effect_tick;
    uint32_t effect_rng;
    int nurse_side;
    int nurse_gesture_frame;
    int nurse_idle_chunk;
    int nurse_idle_ticks;
    int release_committed;
} GameConfinementVisitPresentationState;

typedef struct GameInsuranceNoticeEntry {
    int cause;
    int kind;
    int player;
    int company;
    int chairman;
    int amount;
    int duration;
} GameInsuranceNoticeEntry;

typedef struct GameInsurancePresentationState {
    GameInsuranceNoticeEntry queue[RICH4_INSURANCE_NOTICE_CAPACITY];
    int head;
    int count;
    uint32_t until;
} GameInsurancePresentationState;

/* Lottery-counter phases are app state, not an SDL presentation constant. */
enum {
    RICH4_LOTTERY_VISIT_INTRO_1 = 1,
    RICH4_LOTTERY_VISIT_INTRO_2,
    RICH4_LOTTERY_VISIT_SELECT,
    RICH4_LOTTERY_VISIT_INSUFFICIENT,
    RICH4_LOTTERY_VISIT_EXIT
};

enum {
    RICH4_ROULETTE_SPINNING = 0,
    RICH4_ROULETTE_DECELERATING = 1,
    RICH4_ROULETTE_LANDED = 2
};

enum {
    RICH4_SHOP_OWNER_GIFT = 1,
    RICH4_SHOP_OWNER_SPEECH,
    RICH4_SHOP_GREETING,
    RICH4_SHOP_SLIDE_IN,
    RICH4_SHOP_PROMPT,
    RICH4_SHOP_ACTIVE,
    RICH4_SHOP_FEEDBACK,
    RICH4_SHOP_EXIT_MESSAGE
};

enum {
    RICH4_SHOP_HIT_NONE = -1,
    RICH4_SHOP_HIT_TAB = -2,
    RICH4_SHOP_HIT_EXIT = -3,
    RICH4_SHOP_HIT_INVENTORY_BASE = 0x100
};

typedef struct GameLotteryPresentationState {
    int active;
    int phase;
    int hover;
    int selected;
    int speech_index;
    int speech_active;
    int voice_started;
    int animation_frame;
    int animation_complete;
    int blink_frame;
    int gesture_chunk;
    int gesture_ticks;
    uint32_t speech_tick;
    uint32_t effect_tick;
    uint32_t effect_rng;
} GameLotteryPresentationState;

typedef struct GameShopPresentationState {
    int active;
    int player;
    int phase;
    int gift_kind;
    int gift_id;
    int gift_price;
    int mode;
    int pressed;
    int page_loaded[2];
    int message_index;
    int message_resume_phase;
    int clerk_face_chunk;
    int clerk_mouth_chunk;
    int clerk_idle_state;
    int clerk_idle_variant;
    int clerk_idle_step;
    int clerk_mouth_ticks;
    int clerk_timer_phase;
    int ui_started;
    int music_started;
    int card_count;
    int card_merchandise[15];
    int card_sold[15];
    int tool_merchandise[8];
    int tool_sold[8];
    int inventory_tool_slot[15];
    int left_x;
    int inventory_x;
    int left_velocity;
    int inventory_velocity;
    uint32_t next_tick;
    uint32_t gift_until;
    uint32_t message_until;
    uint32_t clerk_tick;
} GameShopPresentationState;

typedef enum Rich4StockPanelPage {
    RICH4_STOCK_PANEL_QUOTES = 0,
    RICH4_STOCK_PANEL_HOLDINGS,
    RICH4_STOCK_PANEL_DETAIL
} Rich4StockPanelPage;

enum {
    RICH4_STOCK_HIT_NONE = -1,
    RICH4_STOCK_HIT_TOGGLE = 1,
    RICH4_STOCK_HIT_BUY,
    RICH4_STOCK_HIT_SELL,
    RICH4_STOCK_HIT_INFO,
    RICH4_STOCK_HIT_CLOSE,
    RICH4_STOCK_HIT_ROW_BASE = 100
};

typedef struct GameStockInteractionPresentationState {
    int active;
    Rich4StockPanelPage page;
    int selected_stock;
    int pressed_control;
    int quantity_active;
    int quantity_pressed_control;
    GameCommandType trade_type;
    int quantity;
    int maximum;
    int message;
} GameStockInteractionPresentationState;

enum {
    RICH4_STOCK_MESSAGE_NONE = 0,
    RICH4_STOCK_MESSAGE_LIMIT_UP,
    RICH4_STOCK_MESSAGE_LIMIT_DOWN
};

typedef enum Rich4SaleInteractionPage {
    RICH4_SALE_PAGE_BROWSE = 0,
    RICH4_SALE_PAGE_ITEMS,
    RICH4_SALE_PAGE_PRICE,
    RICH4_SALE_PAGE_CONFIRM,
    RICH4_SALE_PAGE_PURCHASE_CONFIRM
} Rich4SaleInteractionPage;

typedef enum Rich4SaleInteractionCategory {
    RICH4_SALE_CATEGORY_STOCK = 0,
    RICH4_SALE_CATEGORY_PROPERTY,
    /* 0x427C21 posts 0x411 to the 13-tool panel and 0x412 to the card list. */
    RICH4_SALE_CATEGORY_TOOL,
    RICH4_SALE_CATEGORY_CARD,
    RICH4_SALE_CATEGORY_COUNT
} Rich4SaleInteractionCategory;

/* Sale panel state mirrors only user-visible selection and numeric input.
 * Market ownership and offers stay in GameState and change only by command. */
typedef struct GameSaleInteractionPresentationState {
    int active;
    Rich4SaleInteractionPage page;
    Rich4SaleInteractionCategory category;
    int selected_row;
    int page_offset;
    int selected_seller;
    int selected_slot;
    GameDomainRef selected_item;
    int item_quantity_max;
    int ask_price;
    int quantity;
    int edit_quantity;
    int pressed_control;
    int message;
} GameSaleInteractionPresentationState;

#endif
