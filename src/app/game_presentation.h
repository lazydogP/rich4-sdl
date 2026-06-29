#ifndef RICH4_APP_GAME_PRESENTATION_H
#define RICH4_APP_GAME_PRESENTATION_H
#include <stdint.h>
#include "app/animation_clock.h"
#include "core/game_command.h"
#include "core/game_constants.h"
#include "core/game_map_dynamic_object.h"
#include "core/game_state.h"
#include "core/turn_rotation.h"
#include "app/ai_settings_state.h"
#include "app/card_panel_state.h"
#include "app/game_presentation_feature_states.h"
#include "app/game_presentation_handler.h"
#include "app/options_flow.h"
#include "app/tool_panel_state.h"
#include "data/character_speech.h"
/* App-owned camera and interaction presentation state; no SDL dependency.
 * Resource and domain state remain borrowed from their owning services. */
enum { RICH4_GAME_INTRO_AVI_NAME_MAX = 16 };
/* Map auxiliary records carry a fixed 16-byte company name at +0x04.
 * Keep this transient copy in presentation state so the original roulette
 * can use the map's actual string without adding it to save/replay state. */
enum { RICH4_MAP_COMPANY_NAME_BYTES = 16 };
typedef enum Rich4PresentationMode {
    RICH4_PRESENTATION_NONE = 0,
    RICH4_PRESENTATION_SPEECH,
    RICH4_PRESENTATION_DICE,
    RICH4_PRESENTATION_PROPERTY,
    RICH4_PRESENTATION_BANK,
    RICH4_PRESENTATION_INSURANCE,
    RICH4_PRESENTATION_LOTTERY,
    RICH4_PRESENTATION_SHOP,
    RICH4_PRESENTATION_CONFINEMENT,
    RICH4_PRESENTATION_POINT_MINIGAME,
    RICH4_PRESENTATION_ROULETTE,
    RICH4_PRESENTATION_MAGIC_HOUSE,
    RICH4_PRESENTATION_BANKRUPTCY,
    RICH4_PRESENTATION_AUCTION,
    RICH4_PRESENTATION_TOOL_ANIMATION,
    RICH4_PRESENTATION_GOD
} Rich4PresentationMode;
typedef union Rich4PresentationPayload {
    struct { int player; int kind; } speech;
    struct { int count; int result; } dice;
    struct { int actor; int land; int amount; } property;
    struct { int player; int feature; } feature;
} Rich4PresentationPayload;
typedef enum Rich4PropertyPresentationPhase {
    GAME_PROPERTY_PRESENT_NONE = 0,
    GAME_PROPERTY_PRESENT_FLASH,
    GAME_PROPERTY_PRESENT_AMOUNT,
    GAME_PROPERTY_PRESENT_PAYER,
    GAME_PROPERTY_PRESENT_RECEIVER,
    GAME_PROPERTY_PRESENT_RELATED_RECEIVER,
    GAME_PROPERTY_PRESENT_COMMIT,
    GAME_PROPERTY_PRESENT_BLOCKED_TEXT,
    GAME_PROPERTY_PRESENT_BLOCKED_SPEECH,
    GAME_PROPERTY_PRESENT_BLOCKED_COMPLETE,
    GAME_PROPERTY_PRESENT_BEGGAR_AMOUNT,
    /* 0x40FA61 -> 0x440CAC: attached god refuses a confirmed trade.
     * GamePropertyPresentationState.amount carries the god type while this
     * transient phase is active. */
    GAME_PROPERTY_PRESENT_GOD_TRADE_REJECTION,
    GAME_PROPERTY_PRESENT_GOD_FEE,
    GAME_PROPERTY_PRESENT_GOD_BUILD_TEXT,
    GAME_PROPERTY_PRESENT_GOD_BUILD_CHOICE,
    GAME_PROPERTY_PRESENT_GOD_BUILD_COMMIT,
    GAME_PROPERTY_PRESENT_GOD_BUILD_FINISH,
    GAME_PROPERTY_PRESENT_INSURANCE_FREE,
    GAME_PROPERTY_PRESENT_INSURANCE_FREE_CONFIRM,
    GAME_PROPERTY_PRESENT_INSURANCE_REDIRECT,
    GAME_PROPERTY_PRESENT_INSURANCE_CARD,
    GAME_PROPERTY_PRESENT_INSURANCE_TARGET,
    GAME_PROPERTY_PRESENT_INSURANCE_REDIRECT_NOTICE,
    GAME_PROPERTY_PRESENT_INSURANCE_CARD_SPEECH,
    GAME_PROPERTY_PRESENT_INSURANCE_VICTIM_SPEECH,
    GAME_PROPERTY_PRESENT_INSURANCE_SUBSTITUTE,
    GAME_PROPERTY_PRESENT_INSURANCE_SUBSTITUTE_NOTICE
} Rich4PropertyPresentationPhase;
typedef struct Rich4PresentationActivity {
    int bank_settlement_active;
    int bank_loan_due_active;
    int tool_placement_active;
    int property_speech_waiting;
    int tool_animation_queue_active;
    int tool_trigger_turn_pending;
    int tool_robot_active;
    int special_actor_active;
    int confinement_exit_active;
    int monster_card_active;
    int special_reward_active;
    int bank_rejection_active;
    int special_place_notice_active;
    int insurance_notice_active;
    int special_event_active;
    int lottery_active;
    int confinement_visit_active;
    int point_minigame_active;
    int shop_active;
    int facility_roulette_active;
    int company_issue_active;
    int magic_house_active;
    int bankruptcy_active;
    int auction_active;
    int god_acquisition_active;
    int speech_active;
    int confinement_status_active;
    int dice_popup_active;
} Rich4PresentationActivity;
/* The platform adapter supplies the clock, assets and side-effect sinks, but
 * app owns which presentation update gets first chance in a frame. */
typedef enum Rich4PresentationUpdateStep {
    RICH4_PRESENTATION_UPDATE_FEATURES = 0,
    RICH4_PRESENTATION_UPDATE_BANK,
    RICH4_PRESENTATION_UPDATE_TOOL_PLACEMENT,
    RICH4_PRESENTATION_UPDATE_PROPERTY_SPEECH,
    RICH4_PRESENTATION_UPDATE_TOOL_QUEUE,
    RICH4_PRESENTATION_UPDATE_TOOL_TRIGGER,
    RICH4_PRESENTATION_UPDATE_TOOL_ROBOT,
    RICH4_PRESENTATION_UPDATE_GOD,
    RICH4_PRESENTATION_UPDATE_ENGINEERING_ATTACK
} Rich4PresentationUpdateStep;
typedef struct GameIntroPresentationState {
    int active;
    int interrupt_requested;
    int player;
    int frame;
    uint32_t tick;
    int jump_sound_pending;
    int door_sound_pending;
    int avi_attempted;
    int avi_skipped_no_backend;
    int avi_original_stage;
    int avi_original_map;
    char avi_name[RICH4_GAME_INTRO_AVI_NAME_MAX];
} GameIntroPresentationState;

typedef struct GameDicePresentationState {
    int status_visible;
    int action_hint_active;
    int input_refresh_active;
    int status_x;
    int status_y;
    int status_dragging;
    int status_drag_offset_x;
    int status_drag_offset_y;
    int status_variant;
    uint32_t status_tick;
    int popup_active;
    int count;
    int faces[3];
    int animation_frame;
    uint32_t animation_tick;
    int result_visible;
    uint32_t result_tick;
    int sound_pending;
    int cache_reset_pending;
} GameDicePresentationState;

typedef struct GameSpeechPresentationState {
    int active;
    int player;
    int kind;
    int type;
    int after_phase;
    uint32_t tick;
    int voice_started;
    int skip_requested;
    Rich4CharacterSpeechSnapshot current_snapshot;
    Rich4CharacterSpeech restored_current;
    const Rich4CharacterSpeech *current;
    Rich4CharacterSpeechSnapshot last_text_snapshot;
    int last_text_rehydrated;
    const char *last_text;
} GameSpeechPresentationState;

typedef struct GamePropertyPresentationState {
    int sound_pending;
    int max_level_pending;
    int max_level_active;
    int max_level_frame;
    uint32_t max_level_tick;
    int max_level_sound_pending;
    Rich4PropertyPresentationPhase phase;
    uint32_t tick;
    int actor;
    int owner;
    GameDomainKind property_kind;
    int land;
    int group;
    int amount;
    int related_owner;
    int owner_amount;
    int related_amount;
    int collection_block_reason;
    int flash_count;
    int flash_frame;
    int flash_presented_frame;
    int flash_adjust;
    int purchase_delay_pending;
    /* Transient 0x41D709/0x40F381 continuation; saving is disabled until done. */
    int god_type;
    int god_original_amount;
    int god_node;
    int god_choice;
    int god_build_animation_started;
    int god_previous_building_type;
    int god_previous_building_level;
    /* Blocking 0x41AF0C..0x41B022 locals; never persisted mid-charge. */
    int insurance_active;
    int insurance_payer;
    int insurance_card;
    int insurance_target;
    int insurance_sound_pending;
} GamePropertyPresentationState;

typedef struct GameRouteInteractionPresentationState {
    int active;
    int hover;
    int pressed;
    GameCommand accept;
    GameCommand decline;
} GameRouteInteractionPresentationState;

typedef struct GameBankInteractionPresentationState {
    int mode;
    int amount;
    int pressed_control;
    int special_page;
    int amount_entry;
    int card_x;
    int card_velocity;
    int face_chunk;
    int mouth_chunk;
    int mouth_ticks;
    int eye_mode;
    int eye_phase;
    int eye_current;
    int animation_page;
    Rich4BankInteractionStage stage;
    int success_close;
    Rich4CharacterSpeechSnapshot speech_snapshot;
    Rich4CharacterSpeech restored_speech;
    const Rich4CharacterSpeech *speech;
    int speech_pending;
    uint32_t started;
    uint32_t anim_tick;
    uint32_t speech_tick;
} GameBankInteractionPresentationState;

/* Transient 0x40D7C4 EA2=3 state. Saving is blocked while it owns the turn. */
typedef struct GameEngineeringAttackState {
    int active;
    int pending_return;
    int initialized;
    int player;
    int node;
    int property;
    int facility;
    int saved_direction;
    int world_x;
    int world_y;
    int hold_ticks;
    int cursor;
    int actor_frame;
    int effect_frame;
    uint32_t tick;
} GameEngineeringAttackState;

typedef struct GameActorPresentationState {
    int direction;
    int walk_frame;
    int visual_x;
    int visual_y;
    int move_active;
    int move_from_node;
    int move_to_node;
    int move_from_x;
    int move_from_y;
    int move_to_x;
    int move_to_y;
    int move_progress;
    int move_distance;
    int intro_visible;
    int first_turn_landing_pending;
    int dirty_flags;
} GameActorPresentationState;

/* Transient locals from 0x40D5A5/0x40D7C4; saving waits for entry to finish. */
typedef struct GameHotelEntryPresentationState {
    int pending, active, player, facility, phase, wait_ticks, saved_direction;
    uint32_t tick;
} GameHotelEntryPresentationState;

typedef struct GameSpecialActorPresentationState {
    int active;
    int slot;
    int path_count;
    int path_index;
    int path_nodes[TURN_SPECIAL_AUTO_MAX_STEPS + 1];
    int visual_x;
    int visual_y;
    int move_from_x;
    int move_from_y;
    int move_to_x;
    int move_to_y;
    int move_progress;
    int move_distance;
    int direction;
    int animation_frame;
    int resource_part;
    int steps_remaining;
    uint32_t last_step_tick;
    GameSpecialActorActionBatch action_batch;
    int action_index;
    uint32_t action_until;
} GameSpecialActorPresentationState;

/* 0x417559 keeps the map information bubble as a transient cursor result.
 * The app stores only primitive display state; hit testing and rendering stay
 * in the SDL map adapter. */
typedef struct GameMapInfoPresentationState {
    int active;
    int press_pending;
    int object_id;
    int cursor_x;
    int cursor_y;
} GameMapInfoPresentationState;

typedef enum GameSaveLoadPanelMode {
    GAME_SAVE_LOAD_PANEL_CLOSED = 0,
    GAME_SAVE_LOAD_PANEL_SAVE,
    GAME_SAVE_LOAD_PANEL_LOAD
} GameSaveLoadPanelMode;

typedef struct GameSaveLoadSlotMetadata {
    int year, month, day, stage, map;
    int characters[GAME_MAX_PLAYERS];
} GameSaveLoadSlotMetadata;

typedef struct GameSaveLoadPresentationState {
    GameSaveLoadPanelMode mode;
    int selected_row;
    int pressed_row;
    int save_confirmation_active;
    uint32_t save_confirmation_started;
    /* Runtime-only: the ended session's active player is restored if the
     * post-ending load picker is cancelled. Save-load presentation is not
     * part of the save codec. */
    int terminal_previous_player;
    unsigned int valid_slot_mask;
    int slot_elapsed_days[6];
    int slot_player_counts[6];
    int slot_year[6];
    int slot_month[6];
    int slot_day[6];
    int slot_stage[6];
    int slot_map[6];
    int slot_characters[6][GAME_MAX_PLAYERS];
} GameSaveLoadPresentationState;

typedef struct GameConfinementExitPresentationState {
    int active;
    int player;
    int kind;
    uint32_t last_step_tick;
} GameConfinementExitPresentationState;

/* 0x40E32C's old-god release animation is presentation-only.  Keep it
 * outside GameMapDynamicObjectState: the latter is the original 24-byte
 * map-object record and must remain layout-compatible with the resource
 * renderer. */
typedef struct GameGodDetachmentPresentationState {
    int active;
    int player;
    int object_slot;
    int god_type;
    int frame;
    int frame_count;
    int frame_delay_ms;
    int direction;
    int world_x;
    int world_y;
    int started;
    uint32_t frame_tick;
    int speech_pending;
    /* Transient; active detachment cannot be saved. 0x40E476 Effect[54]. */
    int sound_pending;
} GameGodDetachmentPresentationState;

/* The domain terminal event has already committed winner/reason/assets.  This
 * snapshot lets the SDL boundary show the resource-backed ending without
 * reopening or mutating the core state. */
typedef struct GameEndingPresentationState {
    int active;
    /* 0x407842 continuation/load prompt shown when the last human goes
     * bankrupt.  It uses the ending input path but is not GAME_OVER. */
    int continuation_active;
    int winner_player;
    int winner_character;
    int reason;
    int assets;
    int ending_video_pending;
    /* The original terminal path speaks once after selecting the winner;
     * SDL consumes this one-shot request through the normal speech flow. */
    int winner_voice_pending;
    /* Platform-only timing/lifecycle facts.  They deliberately stay out of
     * the save codec: a save made from the terminal prompt resumes through
     * the ordinary load UI, not halfway through an AVI/JUMP transition. */
    uint32_t continuation_started;
    int jump_transition_active;
    int jump_transition_code;
    uint32_t jump_transition_started;
    int continuation_confirmation_active;
    int continuation_confirmation_ready;
    uint32_t continuation_confirmation_started;
    int continuation_confirmation_frame;
} GameEndingPresentationState;

/* 0x447D97: AI announces a selected tool before entering its handler. */
typedef enum GameAiToolPhase {
    GAME_AI_TOOL_NONE = 0,
    GAME_AI_TOOL_NOTICE,
    GAME_AI_TOOL_SPEECH,
    GAME_AI_TOOL_COMMAND,
    GAME_AI_TOOL_EFFECTS
} GameAiToolPhase;
typedef struct GameAiToolPresentationState {
    GameAiToolPhase phase;
    GameCommand command;
    uint32_t started;
    int skip_requested;
} GameAiToolPresentationState;

typedef struct GamePresentationState {
    Rich4PresentationMode mode;
    Rich4PresentationMode previous_mode;
    Rich4PresentationPayload payload;
    uint32_t transition_serial;
    /* Transient playback clocks; save files retain semantic frame state. */
    Rich4AnimationClock animation_clock[RICH4_ANIMATION_COUNT];
    GameIntroPresentationState intro;
    GameDicePresentationState dice;
    GameSpeechPresentationState speech;
    GameAiToolPresentationState ai_tool;
    GamePropertyPresentationState property;
    GameActorPresentationState actors[GAME_MAX_PLAYERS];
    GameEngineeringAttackState engineering_attack;
    GameSpecialActorPresentationState special_actor;
    GameConfinementExitPresentationState confinement_exit;
    GameHotelEntryPresentationState hotel_entry;
    GameGodDetachmentPresentationState god_detachment;
    GameEndingPresentationState ending;
    GameSpecialRewardPresentationState special_reward;
    GameGodAcquisitionPresentationState god_acquisition;
    GameBankRejectionPresentationState bank_rejection;
    GameSpecialPlaceNoticePresentationState special_place_notice;
    GameBankruptcyPresentationState bankruptcy;
    GameCompanyIssuePresentationState company_issue;
    GameFacilityRoulettePresentationState facility_roulette;
    char map_company_names[GAME_MAX_MAP_PROPERTIES]
        [RICH4_MAP_COMPANY_NAME_BYTES + 1];
    GameBankSettlementPresentationState bank_settlement;
    GamePeriodicFinancePresentationState periodic_finance;
    GameBankLoanDuePresentationState bank_loan_due;
    GameConfinementStatusPresentationState confinement_status;
    GameConfinementVisitPresentationState confinement_visit;
    GameInsurancePresentationState insurance;
    GameLotteryPresentationState lottery;
    GameShopPresentationState shop_visit;
    GamePointMinigamePresentationState point_minigame;
    GameMagicHousePresentationState magic_house;
    GameAuctionPresentationState auction;
    GameToolAnimationRequest tool_animation[GAME_EVENT_QUEUE_CAPACITY];
    int tool_animation_head;
    int tool_animation_count;
    int tool_trigger_turn_pending;
    GameToolPlacementAnimation tool_placement_animation;
    uint32_t tool_robot_tick;
    /* Transient actor-8 movement (0x40C48C); saving is disabled in motion. */
    struct {
        int initialized;
        int progress;
        int animation_frame;
        int arrived;
        /* 0x40D92B: five pulses, then 0x418E7F's three return pulses. */
        int end_wait;
        int direction;
        int world_x;
        int world_y;
    } tool_robot_motion;
    GameMonsterCardPresentationState monster_card;
    GameSpecialEventPresentationState special_event;
    GameMapDynamicObjectState dynamic_objects[GAME_MAP_DYNAMIC_OBJECT_COUNT];
    GameMapInfoPresentationState map_info;
    GameRouteInteractionPresentationState route_interaction;
    GameBankInteractionPresentationState bank_interaction;
    GameStockInteractionPresentationState stock_interaction;
    GameSaleInteractionPresentationState sale_interaction;
    GameSaveLoadPresentationState save_load;
    /* Set by the shared turn-rotation flow after the original full-round
     * boundary; the SDL adapter consumes it when auto-save is enabled. */
    int auto_save_pending;
    Rich4AiSettingsState ai_settings;
    /* Player-information modal.  0x424492 opens this independently from
     * the right-side HUD tabs; selected_player starts at current_player. */
    struct {
        int active;
        int selected_player;
        /* Panel.mkf[9] chunks 0, 1 and 2: 資產清單, 地產清單, 股票清單. */
        int tab;
        /* 0x423841 and 0x4225A3: the five property-list filters and
         * their ten-row page cursor. */
        int property_filter;
        int property_page;
        /* 0x48C284 in 0x423CF3: 1 close, 2..4 tabs, 5/6 page arrows,
         * 7 player selector.  Zero is the ordinary, unpressed state. */
        int pressed_control;
    } player_info;
    CardPanelState card_panel;
    Rich4OptionsPanelState options;
    ToolPanelState tool_panel;
    uint32_t movement_tick;
    int view_world_x;
    int view_world_y;
    int has_view_override;
    int dragging_minimap;
    int minimap_rotate_pressed; /* 0x48BE28: 1 left, 2 right until button-up. */
    int map_direction;
    int active_info_tab;
    int player_info_tabs[GAME_MAX_PLAYERS]; /* 0x48BE24: one page per player. */
    int top_button_hover;
    int top_button_pressed;
} GamePresentationState;

/* 0x40EAD7's transfer loop blocks inside each bankruptcy/auction before
 * charging the next opponent. Those nested presentations own input. */
static inline int rich4_presentation_god_owns_frame(const GamePresentationState *p)
{
    if (p != NULL && (p->god_detachment.speech_pending ||
        (p->god_acquisition.arrival_speech_phase == 2 && p->speech.active))) return 0;
    return p != NULL && p->god_acquisition.active &&
        !(p->god_acquisition.money.active && p->god_acquisition.money.phase >= 10 &&
          (p->bankruptcy.active || p->bankruptcy.awaiting_auction ||
           p->auction.active || p->speech.active || p->ending.active));
}

void rich4_presentation_initialize(GamePresentationState *state);
int rich4_presentation_rebind_speech(GamePresentationState *state);
int rich4_presentation_speech_saveable(
    const GamePresentationState *state);
void rich4_presentation_save_load_panel_begin(
    GameSaveLoadPresentationState *state, GameSaveLoadPanelMode mode,
    unsigned int valid_slot_mask, const int slot_elapsed_days[6],
    const int slot_player_counts[6]);
void rich4_presentation_save_load_panel_set_metadata(
    GameSaveLoadPresentationState *state,
    const GameSaveLoadSlotMetadata metadata[6]);
void rich4_presentation_save_load_panel_close(
    GameSaveLoadPresentationState *state);
void rich4_presentation_save_load_panel_set_mode(
    GameSaveLoadPresentationState *state, GameSaveLoadPanelMode mode);
void rich4_presentation_save_load_panel_press(
    GameSaveLoadPresentationState *state, int row);
void rich4_presentation_save_load_panel_select(
    GameSaveLoadPresentationState *state, int row);
void rich4_presentation_save_load_panel_confirm_saved(
    GameSaveLoadPresentationState *state, int row, uint32_t now);
int rich4_presentation_save_load_panel_finish_confirmation(
    GameSaveLoadPresentationState *state, uint32_t now);
int rich4_presentation_take_auto_save_pending(
    GamePresentationState *state);
Rich4PresentationMode rich4_presentation_mode_resolve(
    const Rich4PresentationActivity *activity);
Rich4PresentationActivity rich4_presentation_activity_from_state(
    const GamePresentationState *presentation, const GameState *state);
Rich4PresentationUpdateStep rich4_presentation_update_step(
    const GamePresentationState *presentation, const GameState *state);
Rich4PresentationHandler rich4_presentation_handler_for_mode(
    const GamePresentationState *presentation, Rich4PresentationMode mode);
void rich4_presentation_transition(GamePresentationState *state,
                                   Rich4PresentationMode mode,
                                   const Rich4PresentationPayload *payload);
void rich4_presentation_set_view_override(
    GamePresentationState *state, int world_x, int world_y);
void rich4_presentation_set_view_position(
    GamePresentationState *state, int world_x, int world_y);
void rich4_presentation_clear_view_override(GamePresentationState *state);

#endif
