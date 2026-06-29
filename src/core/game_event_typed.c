#include "core/game_event_payload.h"
#include "core/game_event_internal.h"

#include <stddef.h>

static GameDomainRef none_domain_ref(void)
{
    GameDomainRef value;
    value.kind = GAME_DOMAIN_NONE;
    value.id = -1;
    return value;
}

GameEvent rich4_game_event_route_move_resolved(
    int player, int result, int origin, int next_node)
{
    Rich4GameEventFields fields;
    fields.player = player;
    fields.subject = result;
    fields.value = origin;
    fields.value2 = next_node;
    fields.flags = 0u;
    fields.subject_ref = none_domain_ref();
    fields.target_ref = none_domain_ref();
    return rich4_game_event_route_move_resolved_from_fields(fields);
}

int rich4_game_event_emit_route_move_resolved(
    GameEventQueue *queue, int player, int result, int origin, int next_node)
{
    GameEvent event = rich4_game_event_route_move_resolved(
        player, result, origin, next_node);
    return rich4_game_event_queue_push(queue, &event);
}

GameEvent rich4_game_event_route_event_resolved(
    int player, int phase, int result, int node, unsigned int object_id)
{
    Rich4GameEventFields fields;
    fields.player = player;
    fields.subject = phase;
    fields.value = result;
    fields.value2 = node;
    fields.flags = object_id;
    fields.subject_ref = none_domain_ref();
    fields.target_ref = none_domain_ref();
    return rich4_game_event_route_event_resolved_from_fields(fields);
}

int rich4_game_event_emit_route_event_resolved(
    GameEventQueue *queue, int player, int phase, int result, int node,
    unsigned int object_id)
{
    GameEvent event = rich4_game_event_route_event_resolved(
        player, phase, result, node, object_id);
    return rich4_game_event_queue_push(queue, &event);
}

GameEvent rich4_game_event_interaction_requested(
    int player, int subject, GameDomainRef subject_ref, GameDomainRef target,
    int accept_type, int decline_type, unsigned int turn_token)
{
    Rich4GameEventFields fields;
    fields.player = player;
    fields.subject = subject;
    fields.value = accept_type;
    fields.value2 = decline_type;
    fields.flags = turn_token;
    fields.subject_ref = subject_ref;
    fields.target_ref = target;
    return rich4_game_event_interaction_requested_from_fields(fields);
}

int rich4_game_event_emit_interaction_requested(
    GameEventQueue *queue, int player, int subject, GameDomainRef subject_ref,
    GameDomainRef target, int accept_type, int decline_type,
    unsigned int turn_token)
{
    GameEvent event = rich4_game_event_interaction_requested(
        player, subject, subject_ref, target, accept_type, decline_type,
        turn_token);
    return rich4_game_event_queue_push(queue, &event);
}

GameEvent rich4_game_event_auction_request(
    int player, GameDomainRef property, int seller, int manage_music,
    int trigger, unsigned int flags)
{
    Rich4GameEventFields fields;
    fields.player = player;
    fields.subject = 0;
    fields.value = manage_music;
    fields.value2 = trigger;
    fields.flags = flags;
    fields.subject_ref = property;
    fields.target_ref = seller >= 0 ?
        rich4_game_domain_ref(GAME_DOMAIN_PLAYER, seller) : none_domain_ref();
    return rich4_game_event_auction_requested_from_fields(fields);
}

Rich4GameEventFields rich4_game_event_fields(const GameEvent *event)
{
    Rich4GameEventFields fields;
    fields.player = 0;
    fields.subject = 0;
    fields.value = 0;
    fields.value2 = 0;
    fields.flags = 0u;
    fields.subject_ref = none_domain_ref();
    fields.target_ref = none_domain_ref();
    if (event == NULL || !rich4_game_event_is_valid(event)) return fields;
    fields.player = event->player;
    fields.subject = rich4_game_event_subject(event);
    fields.value = rich4_game_event_value(event);
    fields.value2 = rich4_game_event_value2(event);
    fields.flags = rich4_game_event_flags(event);
    fields.subject_ref = rich4_game_event_subject_ref(event);
    fields.target_ref = rich4_game_event_target_ref(event);
    return fields;
}

#define RICH4_DEFINE_TYPED_EVENT(name, expected_type) \
    GameEvent rich4_game_event_##name##_from_fields(Rich4GameEventFields fields) \
    { \
        return rich4_game_event_fields_internal(expected_type, fields); \
    } \
    int rich4_game_event_##name##_emit(GameEventQueue *queue, int player, \
                                       int subject, int value, int value2, \
                                       unsigned int flags) \
    { \
        Rich4GameEventFields fields; \
        fields.player = player; \
        fields.subject = subject; \
        fields.value = value; \
        fields.value2 = value2; \
        fields.flags = flags; \
        fields.subject_ref = none_domain_ref(); \
        fields.target_ref = none_domain_ref(); \
        return rich4_game_event_emit_typed( \
            queue, rich4_game_event_##name##_from_fields, fields); \
    } \
    int rich4_game_event_##name##_is_valid(const GameEvent *event) \
    { \
        return event != NULL && event->type == expected_type && \
               rich4_game_event_is_valid(event); \
    } \
    Rich4GameEventFields rich4_game_event_##name##_fields(const GameEvent *event) \
    { \
        return rich4_game_event_##name##_is_valid(event) \
            ? rich4_game_event_fields(event) : rich4_game_event_fields(NULL); \
    }
RICH4_GAME_EVENT_TYPED_KINDS(RICH4_DEFINE_TYPED_EVENT)
#undef RICH4_DEFINE_TYPED_EVENT
