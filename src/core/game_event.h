#ifndef RICH4_CORE_GAME_EVENT_H
#define RICH4_CORE_GAME_EVENT_H

#include "core/game_command.h"

#define GAME_EVENT_QUEUE_CAPACITY 32

typedef enum GameEventType {
    GAME_EVENT_NONE = 0,
    GAME_EVENT_COMMAND_REJECTED,
    GAME_EVENT_TURN_FLOW_REQUESTED,
    GAME_EVENT_DICE_ROLL_REQUESTED,
    GAME_EVENT_DICE_COUNT_CHANGED,
    GAME_EVENT_ACTOR_REFRESH_REQUESTED,
    GAME_EVENT_SCENE_REFRESH_REQUESTED,
    GAME_EVENT_TEXT_REQUESTED,
    GAME_EVENT_VOICE_REQUESTED,
    GAME_EVENT_STATUS_TICK_REQUESTED,
    GAME_EVENT_STATUS_EXPIRED,
    GAME_EVENT_STATUS_EFFECT_RESOLVED,
    GAME_EVENT_ACTOR_DIRTY_REQUESTED,
    GAME_EVENT_FULL_ROUND_COMPLETED,
    GAME_EVENT_DAY_ADVANCED,
    GAME_EVENT_NORMAL_STATE_CLEARED,
    GAME_EVENT_VICTORY_CANDIDATE,
    GAME_EVENT_ROUTE_MOVE_RESOLVED,
    GAME_EVENT_ROUTE_EVENT_RESOLVED,
    GAME_EVENT_MONEY_TRANSFERRED,
    GAME_EVENT_PROPERTY_OWNER_CHANGED,
    GAME_EVENT_PROPERTY_LEVEL_CHANGED,
    GAME_EVENT_INVENTORY_CHANGED,
    GAME_EVENT_STATUS_CHANGED,
    GAME_EVENT_MARKET_CHANGED,
    GAME_EVENT_INTERACTION_REQUESTED,
    GAME_EVENT_PLAYER_ELIMINATED,
    GAME_EVENT_GAME_ENDED,
    GAME_EVENT_SAVE_RESULT,
    GAME_EVENT_LOAD_RESULT,
    GAME_EVENT_LIQUIDATION_REQUESTED,
    GAME_EVENT_CARD_USED,
    GAME_EVENT_PLAYER_CASH_CHANGED,
    GAME_EVENT_TOOL_USED,
    GAME_EVENT_MAP_OBJECT_CHANGED,
    GAME_EVENT_TRAFFIC_CHANGED,
    GAME_EVENT_SPECIAL_EVENT_PREPARED,
    GAME_EVENT_SPECIAL_EVENT_PRESENTATION_REQUESTED,
    GAME_EVENT_SPECIAL_EVENT_COMMITTED,
    GAME_EVENT_SPECIAL_EVENT_CANCELED,
    GAME_EVENT_PROPERTY_CHARGE_PRESENTATION_REQUESTED,
    /* subject_ref=tool, target_ref=effect center/source, value=Data.MKF
     * resource (0 when no FLC), value2=Effect.MKF resource. */
    GAME_EVENT_TOOL_PRESENTATION_REQUESTED,
    /* Aggregated transactional area result: value=affected count,
     * value2=full-destroy flag. */
    GAME_EVENT_TOOL_AREA_RESOLVED,
    /*
     * Original 0x41982D actions 10..13. subject=route action,
     * value=Data.MKF resource (536 or 537), value2=post-animation hold ms
     * (1000 for point messages, 0 for the gift-card path).
     */
    GAME_EVENT_SPECIAL_PLACE_PRESENTATION_REQUESTED,
    /*
     * Original special-place actions 4/5 (0x43D304/0x43E9A4).
     * subject=GameConfinementKind, value=main Panel.MKF resource
     * (63 prison / 65 hospital), value2=shared Panel.MKF resource 64.
     * The UI selects a target and resumes the frozen route event.
     */
    GAME_EVENT_CONFINEMENT_VISIT_REQUESTED,
    /*
     * Automatic/non-interactive branch shared by original action 6/7/8
     * (0x415457). subject=route action, value=awarded points (50..69),
     * value2=0/1 speech variant from character table 0x48084A.
     * The original shows "得點券%d點" for 2000 ms, then calls player_say.
     */
    GAME_EVENT_POINT_MINIGAME_AUTOMATIC_RESOLVED,
    /*
     * Original action 9 human lottery modal (0x4315CC/0x42F7FC).
     * value/value2=Panel.MKF 12/13; flags carries Panel.MKF 14.
     * Exactly one number may be purchased per visit; purchase or cancel
     * starts the counter's exit speech and then ends the modal.
     */
    GAME_EVENT_LOTTERY_VISIT_REQUESTED,
    /*
     * Original human point minigames, actions 6..8. subject=route action.
     * The SDL presentation owns input/animation and resumes the route with
     * the exact final point result.
     */
    GAME_EVENT_POINT_MINIGAME_REQUESTED,
    /*
     * Original action 15 human department-store/warehouse modal.
     * value/value2=Panel.MKF 10/11.
     */
    GAME_EVENT_SHOP_VISIT_REQUESTED,
    /*
     * 0x4379C9 bank-rejection branch. value is the displayed remaining
     * day count: (days_rejected_by_bank & 0x7f) + 1.  The fixed message
     * remains on screen for 1000 ms and both bank stages are skipped.
     */
    GAME_EVENT_BANK_REJECTION_PRESENTATION_REQUESTED,
    /*
     * 0x44090E facility/company roulette. subject=mode (0..3),
     * value=Panel.MKF resource 68+mode, value2=Data.MKF[517].
     * subject_ref identifies the company/facility being resolved.
     */
    GAME_EVENT_FACILITY_ROULETTE_REQUESTED,
    /*
     * 0x41D1A9 new-share offer after a company landing.
     * subject/company identifies the auxiliary company, value is the
     * per-share price and value2 is the maximum legal quantity.
     */
    GAME_EVENT_COMPANY_STOCK_ISSUE_REQUESTED,
    /*
     * 0x43380A magic-house selector. subject is the randomly selected
     * target condition (0..11), flags is the target-player bit mask.
     * Panel.MKF 18/19/20 are fixed inside the presentation, as in the
     * original function; value/value2 and domain references are unused.
     */
    GAME_EVENT_MAGIC_HOUSE_REQUESTED,
    /* Type-4 company landing. value is the quoted premium before god/card
     * adjustment, value2 is the original 1500 ms display duration. */
    GAME_EVENT_INSURANCE_FEE_PRESENTATION_REQUESTED,
    /* Original claim popup. value is the payout, value2 is 2000 ms and flags
     * is GameInsuranceClaimCause. subject_ref is the first type-4 company. */
    GAME_EVENT_INSURANCE_CLAIM_PRESENTATION_REQUESTED,
    /* 0x439BFA monthly bank settlement. One event is emitted per active
     * player. value=credited interest, value2=new bank balance and flags bit
     * zero marks the original "loan outstanding / no interest" branch.
     * Panel.MKF[25] owns the presentation. */
    GAME_EVENT_BANK_SETTLEMENT_APPLIED,
    /* 0x436A5A ordinary-loan countdown/execution. value=days remaining,
     * subject=principal, value2=GamePaymentResult for the forced branch and
     * flags bit zero marks forced repayment rather than the 3/2/1 reminder. */
    GAME_EVENT_BANK_LOAN_DUE_PRESENTATION_REQUESTED,
    /* Original shared auction 0x43BDE5. subject_ref is the property,
     * target_ref is the seller (NONE means public/system), value is the
     * manage-music flag, value2 is GameAuctionTrigger and flags carries
     * caller-specific unsold policy. */
    GAME_EVENT_AUCTION_REQUESTED,
    /* A road god was attached after a landing or 请神符.  subject=god type,
     * value=one-based dynamic-object slot, value2=previous god type and
     * flags bit 0 marks the first god acquisition for this player. */
    GAME_EVENT_GOD_ACQUIRED,
    /* Listing state changed. subject_ref is the global listing slot,
     * target_ref is the listed item when active; value=ask price,
     * value2=quantity, flags=original listing type. */
    GAME_EVENT_LISTING_CHANGED,
    /* A normal player stopped on a node occupied by an inactive player.
     * target_ref=beggar, value=donation amount, value2=stop node,
     * flags=route turn token. */
    GAME_EVENT_BEGGAR_DONATION_PRESENTATION_REQUESTED,
    /* Original 0x40FA61 -> 0x440CAC after a property YES decision.
     * value=attached god type (7/8/15), value2=display duration (1500 ms),
     * flags=route turn token.  The route remains pending until the popup
     * finishes; no property transaction is committed. */
    GAME_EVENT_GOD_TRADE_REJECTION_PRESENTATION_REQUESTED,
    /* 0x41D08F: Panel[76] then 0x431712. App defers the credits/claim
     * until the corresponding original modal return boundary. */
    GAME_EVENT_PERIODIC_FINANCE_REQUESTED,
    /* Original timed map notices in automatic special-place handlers.
     * subject=GameSpecialPlaceNoticeKind, value=amount/price,
     * value2=hold ms, target_ref=rescued actor or owner gift. */
    GAME_EVENT_SPECIAL_PLACE_NOTICE_REQUESTED
} GameEventType;

typedef enum GameSpecialPlaceNoticeKind {
    GAME_SPECIAL_NOTICE_PRISON_RELEASE = 1,
    GAME_SPECIAL_NOTICE_HOSPITAL_RELEASE,
    GAME_SPECIAL_NOTICE_SHOP_GIFT,
    GAME_SPECIAL_NOTICE_BANK_BORROW,
    GAME_SPECIAL_NOTICE_BANK_REPAY,
    GAME_SPECIAL_NOTICE_BANK_OWNER_SHORTFALL
} GameSpecialPlaceNoticeKind;

/* A single tagged payload boundary for event data that historically occupied
 * anonymous subject/value/value2/flags fields. Callers use named constructors
 * and accessors so the payload is the only scalar/domain data authority. */
typedef enum Rich4GameEventPayloadKind {
    RICH4_GAME_EVENT_PAYLOAD_NONE = 0,
    RICH4_GAME_EVENT_PAYLOAD_SCALAR,
    RICH4_GAME_EVENT_PAYLOAD_DOMAIN,
    RICH4_GAME_EVENT_PAYLOAD_ROUTE
} Rich4GameEventPayloadKind;

typedef struct Rich4GameEventPayload {
    Rich4GameEventPayloadKind kind;
    /* The event kind selects exactly one of these views. All event data lives
     * here; the enclosing GameEvent owns the event type. */
    union {
        struct Rich4GameEventPayloadFields {
            int subject;
            int value;
            int value2;
            unsigned int flags;
            GameDomainRef subject_ref;
            GameDomainRef target_ref;
        } scalar, domain, route;
    } data;
} Rich4GameEventPayload;

typedef struct Rich4GameEventFields {
    int player;
    int subject;
    int value;
    int value2;
    unsigned int flags;
    GameDomainRef subject_ref;
    GameDomainRef target_ref;
} Rich4GameEventFields;

typedef enum GameCommandRejectReason {
    GAME_COMMAND_REJECT_NONE = 0,
    GAME_COMMAND_REJECT_INVALID,
    GAME_COMMAND_REJECT_INACTIVE,
    GAME_COMMAND_REJECT_PLAYER,
    GAME_COMMAND_REJECT_SOURCE,
    GAME_COMMAND_REJECT_STATE,
    GAME_COMMAND_REJECT_VALUE,
    GAME_COMMAND_REJECT_UNSUPPORTED
} GameCommandRejectReason;

typedef struct GameEvent {
    GameEventType type;
    GameCommand command;
    GameCommandRejectReason reject_reason;
    int player;
    Rich4GameEventPayload payload;
} GameEvent;

typedef struct GameEventQueue {
    GameEvent events[GAME_EVENT_QUEUE_CAPACITY];
    int head;
    int count;
    int dropped;
} GameEventQueue;

void rich4_game_event_queue_reset(GameEventQueue *queue);
int rich4_game_event_queue_push(GameEventQueue *queue, const GameEvent *event);
int rich4_game_event_queue_pop(GameEventQueue *queue, GameEvent *out_event);
int rich4_game_event_queue_append(GameEventQueue *queue,
                                  const GameEventQueue *source);
Rich4GameEventPayloadKind rich4_game_event_payload_kind(GameEventType type);
int rich4_game_event_subject(const GameEvent *event);
int rich4_game_event_value(const GameEvent *event);
int rich4_game_event_value2(const GameEvent *event);
unsigned int rich4_game_event_flags(const GameEvent *event);
GameDomainRef rich4_game_event_subject_ref(const GameEvent *event);
GameDomainRef rich4_game_event_target_ref(const GameEvent *event);
int rich4_game_event_kind_is_valid(GameEventType type);
int rich4_game_event_is_valid(const GameEvent *event);

#endif
