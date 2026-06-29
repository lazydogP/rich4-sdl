#include "core/game_event_internal.h"

#include <string.h>

static GameDomainRef none_domain_ref(void)
{
    GameDomainRef value;
    value.kind = GAME_DOMAIN_NONE;
    value.id = -1;
    return value;
}

Rich4GameEventPayloadKind rich4_game_event_payload_kind(GameEventType type)
{
    switch (type) {
    case GAME_EVENT_ROUTE_MOVE_RESOLVED:
    case GAME_EVENT_ROUTE_EVENT_RESOLVED:
        return RICH4_GAME_EVENT_PAYLOAD_ROUTE;
    case GAME_EVENT_INTERACTION_REQUESTED:
    case GAME_EVENT_SPECIAL_EVENT_PRESENTATION_REQUESTED:
    case GAME_EVENT_PROPERTY_CHARGE_PRESENTATION_REQUESTED:
    case GAME_EVENT_TOOL_PRESENTATION_REQUESTED:
    case GAME_EVENT_SPECIAL_PLACE_PRESENTATION_REQUESTED:
    case GAME_EVENT_CONFINEMENT_VISIT_REQUESTED:
    case GAME_EVENT_LOTTERY_VISIT_REQUESTED:
    case GAME_EVENT_POINT_MINIGAME_REQUESTED:
    case GAME_EVENT_SHOP_VISIT_REQUESTED:
    case GAME_EVENT_SPECIAL_PLACE_NOTICE_REQUESTED:
    case GAME_EVENT_FACILITY_ROULETTE_REQUESTED:
    case GAME_EVENT_COMPANY_STOCK_ISSUE_REQUESTED:
    case GAME_EVENT_INSURANCE_FEE_PRESENTATION_REQUESTED:
    case GAME_EVENT_INSURANCE_CLAIM_PRESENTATION_REQUESTED:
    case GAME_EVENT_AUCTION_REQUESTED:
    case GAME_EVENT_LISTING_CHANGED:
    case GAME_EVENT_BEGGAR_DONATION_PRESENTATION_REQUESTED:
    case GAME_EVENT_GOD_TRADE_REJECTION_PRESENTATION_REQUESTED:
        return RICH4_GAME_EVENT_PAYLOAD_DOMAIN;
    case GAME_EVENT_MONEY_TRANSFERRED:
    case GAME_EVENT_PROPERTY_OWNER_CHANGED:
    case GAME_EVENT_PROPERTY_LEVEL_CHANGED:
    case GAME_EVENT_INVENTORY_CHANGED:
    case GAME_EVENT_STATUS_CHANGED:
    case GAME_EVENT_MARKET_CHANGED:
    case GAME_EVENT_PLAYER_CASH_CHANGED:
    case GAME_EVENT_TOOL_USED:
    case GAME_EVENT_MAP_OBJECT_CHANGED:
    case GAME_EVENT_TRAFFIC_CHANGED:
    case GAME_EVENT_STATUS_TICK_REQUESTED:
    case GAME_EVENT_STATUS_EXPIRED:
    case GAME_EVENT_STATUS_EFFECT_RESOLVED:
    case GAME_EVENT_ACTOR_DIRTY_REQUESTED:
    case GAME_EVENT_ACTOR_REFRESH_REQUESTED:
    case GAME_EVENT_SCENE_REFRESH_REQUESTED:
    case GAME_EVENT_TEXT_REQUESTED:
    case GAME_EVENT_VOICE_REQUESTED:
    case GAME_EVENT_DICE_ROLL_REQUESTED:
    case GAME_EVENT_DICE_COUNT_CHANGED:
    case GAME_EVENT_TURN_FLOW_REQUESTED:
    case GAME_EVENT_DAY_ADVANCED:
    case GAME_EVENT_FULL_ROUND_COMPLETED:
    case GAME_EVENT_NORMAL_STATE_CLEARED:
    case GAME_EVENT_VICTORY_CANDIDATE:
    case GAME_EVENT_GAME_ENDED:
    case GAME_EVENT_SAVE_RESULT:
    case GAME_EVENT_LOAD_RESULT:
    case GAME_EVENT_LIQUIDATION_REQUESTED:
    case GAME_EVENT_CARD_USED:
    case GAME_EVENT_PLAYER_ELIMINATED:
    case GAME_EVENT_COMMAND_REJECTED:
    case GAME_EVENT_GOD_ACQUIRED:
        return RICH4_GAME_EVENT_PAYLOAD_SCALAR;
    case GAME_EVENT_NONE:
        return RICH4_GAME_EVENT_PAYLOAD_NONE;
    default:
        return RICH4_GAME_EVENT_PAYLOAD_SCALAR;
    }
}

static const struct Rich4GameEventPayloadFields *event_payload_fields(
    const GameEvent *event)
{
    if (event == NULL || !rich4_game_event_kind_is_valid(event->type) ||
        event->payload.kind != rich4_game_event_payload_kind(event->type))
        return NULL;
    if (event->payload.kind == RICH4_GAME_EVENT_PAYLOAD_DOMAIN)
        return &event->payload.data.domain;
    if (event->payload.kind == RICH4_GAME_EVENT_PAYLOAD_ROUTE)
        return &event->payload.data.route;
    if (event->payload.kind == RICH4_GAME_EVENT_PAYLOAD_SCALAR)
        return &event->payload.data.scalar;
    return NULL;
}

int rich4_game_event_subject(const GameEvent *event)
{
    const struct Rich4GameEventPayloadFields *fields =
        event_payload_fields(event);
    return fields != NULL ? fields->subject : 0;
}

int rich4_game_event_value(const GameEvent *event)
{
    const struct Rich4GameEventPayloadFields *fields =
        event_payload_fields(event);
    return fields != NULL ? fields->value : 0;
}

int rich4_game_event_value2(const GameEvent *event)
{
    const struct Rich4GameEventPayloadFields *fields =
        event_payload_fields(event);
    return fields != NULL ? fields->value2 : 0;
}

unsigned int rich4_game_event_flags(const GameEvent *event)
{
    const struct Rich4GameEventPayloadFields *fields =
        event_payload_fields(event);
    return fields != NULL ? fields->flags : 0u;
}

GameDomainRef rich4_game_event_subject_ref(const GameEvent *event)
{
    const struct Rich4GameEventPayloadFields *fields =
        event_payload_fields(event);
    return fields != NULL ? fields->subject_ref : none_domain_ref();
}

GameDomainRef rich4_game_event_target_ref(const GameEvent *event)
{
    const struct Rich4GameEventPayloadFields *fields =
        event_payload_fields(event);
    return fields != NULL ? fields->target_ref : none_domain_ref();
}

int rich4_game_event_kind_is_valid(GameEventType type)
{
    return type > GAME_EVENT_NONE &&
           type <= GAME_EVENT_SPECIAL_PLACE_NOTICE_REQUESTED;
}

int rich4_game_event_is_valid(const GameEvent *event)
{
    return event_payload_fields(event) != NULL;
}

static void set_payload_fields(GameEvent *event, Rich4GameEventFields fields)
{
    struct Rich4GameEventPayloadFields *payload_fields;
    event->payload.kind = rich4_game_event_payload_kind(event->type);
    if (event->payload.kind == RICH4_GAME_EVENT_PAYLOAD_DOMAIN)
        payload_fields = &event->payload.data.domain;
    else if (event->payload.kind == RICH4_GAME_EVENT_PAYLOAD_ROUTE)
        payload_fields = &event->payload.data.route;
    else if (event->payload.kind == RICH4_GAME_EVENT_PAYLOAD_SCALAR)
        payload_fields = &event->payload.data.scalar;
    else
        return;
    payload_fields->subject = fields.subject;
    payload_fields->value = fields.value;
    payload_fields->value2 = fields.value2;
    payload_fields->flags = fields.flags;
    payload_fields->subject_ref = fields.subject_ref;
    payload_fields->target_ref = fields.target_ref;
}

GameEvent rich4_game_event_fields_internal(GameEventType type,
                                           Rich4GameEventFields fields)
{
    GameEvent event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    event.player = fields.player;
    set_payload_fields(&event, fields);
    return event;
}

GameEvent rich4_game_event_internal(GameEventType type,
                                    int player,
                                    int subject,
                                    int value,
                                    int value2,
                                    unsigned int flags)
{
    Rich4GameEventFields fields;
    fields.player = player;
    fields.subject = subject;
    fields.value = value;
    fields.value2 = value2;
    fields.flags = flags;
    fields.subject_ref = none_domain_ref();
    fields.target_ref = none_domain_ref();
    return rich4_game_event_fields_internal(type, fields);
}

GameEvent rich4_game_domain_event_internal(GameEventType type,
                                           int player,
                                           int subject_value,
                                           GameDomainRef subject,
                                           GameDomainRef target,
                                           int value,
                                           int value2,
                                           unsigned int flags)
{
    Rich4GameEventFields fields;
    fields.player = player;
    fields.subject = subject_value;
    fields.value = value;
    fields.value2 = value2;
    fields.flags = flags;
    fields.subject_ref = subject;
    fields.target_ref = target;
    return rich4_game_event_fields_internal(type, fields);
}

void rich4_game_event_queue_reset(GameEventQueue *queue)
{
    if (queue != NULL) memset(queue, 0, sizeof(*queue));
}

int rich4_game_event_queue_push(GameEventQueue *queue, const GameEvent *event)
{
    int tail;
    if (queue == NULL || event == NULL || !rich4_game_event_is_valid(event))
        return 0;
    if (queue->count >= GAME_EVENT_QUEUE_CAPACITY) {
        queue->dropped++;
        return 0;
    }
    tail = (queue->head + queue->count) % GAME_EVENT_QUEUE_CAPACITY;
    queue->events[tail] = *event;
    queue->count++;
    return 1;
}

int rich4_game_event_queue_pop(GameEventQueue *queue, GameEvent *out_event)
{
    if (queue == NULL || out_event == NULL || queue->count <= 0) return 0;
    *out_event = queue->events[queue->head];
    queue->head = (queue->head + 1) % GAME_EVENT_QUEUE_CAPACITY;
    queue->count--;
    return 1;
}

int rich4_game_event_queue_append(GameEventQueue *queue,
                                  const GameEventQueue *source)
{
    GameEventQueue next;
    int i;
    if (queue == NULL || source == NULL ||
        queue->count + source->count > GAME_EVENT_QUEUE_CAPACITY) return 0;
    next = *queue;
    for (i = 0; i < source->count; i++) {
        int index = (source->head + i) % GAME_EVENT_QUEUE_CAPACITY;
        if (!rich4_game_event_queue_push(&next, &source->events[index])) return 0;
    }
    *queue = next;
    return 1;
}
