#ifndef RICH4_APP_GAME_PRESENTATION_INTERACTIVE_STATES_H
#define RICH4_APP_GAME_PRESENTATION_INTERACTIVE_STATES_H

#include <stdint.h>

#include "core/auction.h"
#include "data/character_speech.h"

typedef enum Rich4AuctionPresentationStage {
    RICH4_AUCTION_PRESENT_IDLE = 0,
    RICH4_AUCTION_PRESENT_INTRO,
    RICH4_AUCTION_PRESENT_RESERVE,
    RICH4_AUCTION_PRESENT_BIDDING,
    RICH4_AUCTION_PRESENT_ACTION,
    RICH4_AUCTION_PRESENT_SOLD_CALL,
    RICH4_AUCTION_PRESENT_WINNER,
    RICH4_AUCTION_PRESENT_UNSOLD
} Rich4AuctionPresentationStage;

/* sub_435062's n7 states.  Keeping the original numeric values makes the
 * bank callback's speech/card/input ordering directly auditable against the
 * disassembly. */
typedef enum Rich4BankInteractionStage {
    RICH4_BANK_PRESENT_IDLE = 0,
    RICH4_BANK_PRESENT_GREETING_1 = 1,
    RICH4_BANK_PRESENT_GREETING_2 = 3,
    RICH4_BANK_PRESENT_READY = 4,
    RICH4_BANK_PRESENT_LOAN_PROMPT = 5,
    RICH4_BANK_PRESENT_LOAN_AMOUNT = 6,
    RICH4_BANK_PRESENT_LOAN_RESULT_1 = 7,
    RICH4_BANK_PRESENT_LOAN_RESULT_2 = 8,
    RICH4_BANK_PRESENT_REPAY_PROMPT = 9,
    RICH4_BANK_PRESENT_REPAY_AMOUNT = 10,
    RICH4_BANK_PRESENT_EXIT = 11,
    RICH4_BANK_PRESENT_RETRACT = 12
} Rich4BankInteractionStage;
#include "core/game_constants.h"
#include "core/game_domain.h"
#include "core/game_state.h"
#include "core/special_actor_action.h"

typedef struct Rich4CharacterSpeech Rich4CharacterSpeech;

enum { GAME_POINT_MINIGAME_OBJECT_COUNT = 16 };

typedef struct GameAuctionPresentationRequest {
    int initiator;
    int seller;
    GameDomainRef target;
    int manage_music;
    int trigger;
    unsigned int flags;
} GameAuctionPresentationRequest;

typedef struct GameAuctionPresentationState {
    int active;
    Rich4AuctionPresentationStage stage;
    int pressed_action;
    int speech_resource;
    int speech_pending;
    int action_tick;
    uint32_t action_until;
    uint32_t action_started;
    int celebration_tick;
    uint32_t celebration_started;
    unsigned int bidder_mask;
    GameAuctionState bidding;
    GameAuctionPresentationRequest request;
    GameAuctionSettlementResult last_settlement;
    GameAuctionPresentationRequest pending[GAME_EVENT_QUEUE_CAPACITY];
    int pending_head;
    int pending_count;
} GameAuctionPresentationState;

enum {
    RICH4_POINT_MINIGAME_INTRO = 1,
    RICH4_POINT_MINIGAME_PLAY,
    RICH4_POINT_MINIGAME_RESULT,
    RICH4_POINT_MINIGAME_READY
};

enum {
    RICH4_MAGIC_HOUSE_INTRO_1 = 0,
    RICH4_MAGIC_HOUSE_INTRO_2,
    RICH4_MAGIC_HOUSE_INTRO_3,
    RICH4_MAGIC_HOUSE_REVEAL,
    RICH4_MAGIC_HOUSE_CONDITION,
    RICH4_MAGIC_HOUSE_PROMPT,
    RICH4_MAGIC_HOUSE_SELECT,
    RICH4_MAGIC_HOUSE_CONFIRM,
    RICH4_MAGIC_HOUSE_FINALE,
    RICH4_MAGIC_HOUSE_TARGET,
    RICH4_MAGIC_HOUSE_FACILITY_CHOICE,
    RICH4_MAGIC_HOUSE_AI_NOTICE
};

enum {
    RICH4_CONFINEMENT_VISIT_SELECT = 1,
    RICH4_CONFINEMENT_VISIT_CONFIRM,
    RICH4_CONFINEMENT_VISIT_PRISON_RESCUED,
    RICH4_CONFINEMENT_VISIT_HOSPITAL_INTRO,
    RICH4_CONFINEMENT_VISIT_HOSPITAL_RELEASE,
    RICH4_CONFINEMENT_VISIT_HOSPITAL_RESCUED,
    RICH4_CONFINEMENT_VISIT_HOSPITAL_RELEASED_WAIT,
    RICH4_CONFINEMENT_VISIT_HOSPITAL_EXIT,
    RICH4_CONFINEMENT_VISIT_INSUFFICIENT
};

typedef struct PointMinigameObject {
    int active;
    int x;
    int y;
    int type;
    int frame;
    int velocity_x;
    int velocity_y;
} PointMinigameObject;

/* 0x413248 / 0x414FCD. Simulation advances after drawing each object;
 * retain that frame separately so rendering never advances the game. */
typedef struct GameCatcherMinigameState {
    int stage;                 /* 0: play, 1: drain, 2: finish */
    int source_frame;
    int drop_frame;
    int secondary_x;
    int bombs;
    int explosion_frame;       /* number of decoded Data[526] frames */
    int move_frames;
    int mouse_anchor;
    int mouse_warp;
    int mouse_initialized;
    int wait_ticks;
    int scene_initialized;
    int source_draw_x;
    int source_draw_chunk;
    int secondary_draw_frame;
    PointMinigameObject draw_objects[GAME_POINT_MINIGAME_OBJECT_COUNT];
} GameCatcherMinigameState;

/* Keep the automatic result and its already-selected speech in the existing
 * saved scalar; resolved modes must not award points or draw RNG again. */
typedef enum Rich4PointMinigameAutomaticMode {
    RICH4_POINT_MINIGAME_INTERACTIVE = 0,
    RICH4_POINT_MINIGAME_AUTOMATIC_UNRESOLVED = 1,
    RICH4_POINT_MINIGAME_AUTOMATIC_RESOLVED_FIRST_SPEECH = 2,
    RICH4_POINT_MINIGAME_AUTOMATIC_RESOLVED_SECOND_SPEECH = 3
} Rich4PointMinigameAutomaticMode;

typedef struct PointMinigameState {
    int active;
    int automatic;
    int music_started;
    int action;
    int player;
    int phase;
    int ticks_remaining;
    int score;
    int result_tier;
    int animation_frame;
    int animation_delay;
    uint32_t next_tick;
    uint32_t finish_until;
    int counters[5];
    int penguin_treasure[81];
    int penguin_player_cell;
    int penguin_target_cell;
    int penguin_next_cell;
    int penguin_move_frame;
    int penguin_dig_frame;
    int penguin_pickup_type;
    int penguin_pickup_frame;
    int balloon_slow_ticks;
    int balloon_speed_mode;
    int catcher_x;
    int catcher_direction;
    int catcher_frame;
    int catcher_source_x;
    int catcher_source_direction;
    int catcher_secondary_active;
    int catcher_secondary_frame;
    int catcher_bomb_active;
    PointMinigameObject object[GAME_POINT_MINIGAME_OBJECT_COUNT];
    GameCatcherMinigameState catcher;
} GamePointMinigamePresentationState;

typedef struct MagicHouseState {
    int active;
    int player;
    int phase;
    int condition;
    unsigned int targets;
    int hover;
    int selected_action;
    int return_to_selection;
    int animation_enabled;
    int modal_music_finished;
    int speech_started;
    int animation_frame;
    int animation_countdown;
    unsigned int pending_targets;
    int target_player;
    int facility_choice[GAME_MAX_PLAYERS];
    int facility_choice_player;
    int facility_choice_hover;
    uint32_t target_until;
    uint32_t phase_until;
    uint32_t phase_started;
    uint32_t animation_tick;
} GameMagicHousePresentationState;

typedef struct ToolAnimationRequest {
    int resource;
    int effect;
    int frame;
    int active;
    int sound_played;
    /* Keep the request alive after the FLC finishes while its ordered
     * follow-up speeches are being played.  The legacy route dispatcher is
     * synchronous, so a later hospital animation must not overtake these
     * speeches. */
    int animation_done;
    /* 0x43ECA5 returns before admission; 524 retains the claim's day count. */
    int confinement_days;
    int explosion_traffic_method;
    int admission_stage; /* 1: days reaction, 2: commit after reaction. */
    int origin_play_state;
    int origin_traffic_method;
    int world_x;
    int world_y;
    int has_world_position;
    /* >=0: pending actor sync; -1: none; -(actor+2): Data[524] has
     * synced and retains that actor for the post-voice dog claim. */
    int after_sync_player;
    int after_speech_player;
    int after_speech_type;
    Rich4CharacterSpeechSnapshot after_speech_snapshot;
    Rich4CharacterSpeech restored_after_speech;
    const Rich4CharacterSpeech *after_speech;
    int after_speech_next_player;
    int after_speech_next_type;
    Rich4CharacterSpeechSnapshot after_speech_next_snapshot;
    Rich4CharacterSpeech restored_after_speech_next;
    const Rich4CharacterSpeech *after_speech_next;
    GameDomainRef target;
    uint32_t started;
    uint32_t until;
} GameToolAnimationRequest;

typedef struct ToolPlacementAnimation {
    int active;
    int slot;
    int tool;
    int from_x;
    int from_y;
    int to_x;
    int to_y;
    uint32_t started;
    uint32_t until;
} GameToolPlacementAnimation;

typedef struct GameMonsterCardPresentationState {
    int active;
    /* Data.MKF resource: 557 for 怪兽卡, 529 for 拆除卡. */
    int resource;
    /* Effect.MKF resource to emit once when an AI animation starts. */
    int sound_resource;
    int frame;
    int speech_player;
    uint32_t started;
    uint32_t until;
} GameMonsterCardPresentationState;

typedef struct GameSpecialEventPresentationState {
    int active;
    int family;
    int resource;
    int actor;
    int logical_id;
    int resolved_id;
    int value;
    unsigned int target_mask;
    int reaction_player;
    int consequence_index;
    int confinement;
    int first_confinement;
    int reaction_stage;
    int pending_confinement_days[GAME_MAX_PLAYERS];
    int phase;
    int frame;
    uint32_t tick;
    uint32_t until;
    GameState pre_state;
    int pre_state_valid;
    GamePlayerState original_players[GAME_MAX_PLAYERS];
    int original_players_valid;
    int commit_seen;
    int committed_amount;
    int defense;
    int cards_gained;
    int player_amount[GAME_MAX_PLAYERS];
    unsigned int reaction_mask;
    unsigned int fortune_card_target_mask;
    int fortune_card_target_player;
    int fortune_choices_done;
    int fortune_defense_ready;
    /* Blocking callback continuations; omitted from the save codec. */
    int fortune_god_notice_done;
    int fortune_passive_resolved;
    int fortune_redirect_target;
    int fortune_passive_card;
    int fortune_choice_hover;
    int fortune_card_targets_seen;
    int scene_refresh_frame;
    int scene_refreshed;
} GameSpecialEventPresentationState;

#endif
