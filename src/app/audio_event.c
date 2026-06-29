#include "app/audio_event.h"

#include <string.h>

#include "app/game_session.h"
#include "app/turn_movement_flow.h"
#include "core/game_types.h"
#include "data/map_data.h"

void rich4_audio_event_queue_init(Rich4AudioEventQueue *queue)
{
    if (queue != NULL) memset(queue, 0, sizeof(*queue));
}

void rich4_audio_event_queue_reset(Rich4AudioEventQueue *queue)
{
    if (queue == NULL) return;
    queue->head = 0;
    queue->count = 0;
    queue->dropped = 0;
}

Rich4AudioEventQueue *rich4_app_session_audio_borrow(GameSession *session)
{
    Rich4AudioEventQueue *queue;
    if (session == NULL) return NULL;
    queue = &session->audio_events;
    if (queue->borrow_depth == 0) {
        queue->head = 0;
        queue->count = 0;
        queue->dropped = 0;
    }
    queue->borrow_depth++;
    return queue;
}

Rich4AudioEventQueue *rich4_app_session_audio_release(GameSession *session)
{
    Rich4AudioEventQueue *queue;
    if (session == NULL) return NULL;
    queue = &session->audio_events;
    if (queue->borrow_depth == 0) return NULL;
    queue->borrow_depth--;
    return queue->borrow_depth == 0 ? queue : NULL;
}

int rich4_audio_event_emit(Rich4AudioEventQueue *queue,
                           Rich4AudioEventType type,
                           int active,
                           int value)
{
    size_t tail;
    if (queue == NULL || type <= RICH4_AUDIO_EVENT_NONE ||
        type >= RICH4_AUDIO_EVENT_COUNT) return 0;
    if (queue->count >= RICH4_AUDIO_EVENT_CAPACITY) {
        queue->dropped++;
        return 0;
    }
    tail = (queue->head + queue->count) % RICH4_AUDIO_EVENT_CAPACITY;
    queue->events[tail].type = type;
    queue->events[tail].active = active;
    queue->events[tail].value = value;
    queue->count++;
    return 1;
}

int rich4_audio_event_pop(Rich4AudioEventQueue *queue,
                          Rich4AudioEvent *out_event)
{
    if (queue == NULL || out_event == NULL || queue->count == 0) return 0;
    *out_event = queue->events[queue->head];
    queue->head = (queue->head + 1) % RICH4_AUDIO_EVENT_CAPACITY;
    queue->count--;
    return 1;
}

int rich4_audio_movement_sound_method(
    const GamePlayerState *player, const MapData *map)
{
    if (player == NULL) return RICH4_MOVEMENT_SOUND_WALK;
    /* 0x40D9C5 selects sound-table slot 15 whenever actor record +1d is
     * nonzero.  sub_40B93B sets that state on a signed/high-bit route and
     * plays the same slot immediately; 0x4823C2 stores Effect.MKF[47]. */
    if (map != NULL && player->node > 0 &&
        (size_t)player->node < map->route_count &&
        (map->routes[player->node].flags & 0x80000000u) != 0)
        return RICH4_MOVEMENT_SOUND_WATER;
    return player->traffic_method & 3;
}

void rich4_app_collect_session_audio_events(
    GameSession *session, const MapData *movement_map,
    Rich4AudioEventQueue *audio_events)
{
    int current;
    int moving, bomb_moving, method;
    const GamePlayerState *moving_player;

    if (session == NULL || audio_events == NULL) return;
    while (session->presentation.dice.sound_pending > 0) {
        rich4_audio_event_emit(audio_events, RICH4_AUDIO_EVENT_DICE, 1, 0);
        session->presentation.dice.sound_pending--;
    }
    while (session->presentation.intro.jump_sound_pending > 0) {
        rich4_audio_event_emit(audio_events, RICH4_AUDIO_EVENT_JUMP, 1, 0);
        session->presentation.intro.jump_sound_pending--;
    }
    while (session->presentation.intro.door_sound_pending > 0) {
        rich4_audio_event_emit(audio_events, RICH4_AUDIO_EVENT_DOOR, 1, 0);
        session->presentation.intro.door_sound_pending--;
    }
    if (session->presentation.property.sound_pending == 1)
        rich4_audio_event_emit(
            audio_events, RICH4_AUDIO_EVENT_PROPERTY_PURCHASE, 1, 0);
    else if (session->presentation.property.sound_pending == 2)
        rich4_audio_event_emit(
            audio_events, RICH4_AUDIO_EVENT_PROPERTY_UPGRADE, 1, 0);
    else if (session->presentation.property.sound_pending == 3)
        rich4_audio_event_emit(
            audio_events, RICH4_AUDIO_EVENT_PROPERTY_TOLL, 1, 0);
    session->presentation.property.sound_pending = 0;
    if (session->presentation.property.insurance_sound_pending &&
        rich4_audio_event_emit(audio_events, RICH4_AUDIO_EVENT_CARD, 1, 0))
        session->presentation.property.insurance_sound_pending = 0;
    while (session->presentation.property.max_level_sound_pending > 0) {
        rich4_audio_event_emit(
            audio_events, RICH4_AUDIO_EVENT_PROPERTY_MAX_LEVEL, 1, 0);
        session->presentation.property.max_level_sound_pending--;
    }
    if (session->presentation.god_detachment.sound_pending &&
        rich4_audio_event_emit(audio_events, RICH4_AUDIO_EVENT_GOD_DEPARTURE, 1, 54))
        session->presentation.god_detachment.sound_pending = 0;
    while (session->presentation.god_acquisition.sound_pending > 0) {
        rich4_audio_event_emit(
            audio_events, RICH4_AUDIO_EVENT_GOD_EFFECT, 1,
            session->presentation.god_acquisition.god_type);
        session->presentation.god_acquisition.sound_pending--;
    }
    if (session->presentation.monster_card.sound_resource != 0) {
        rich4_audio_event_emit(
            audio_events, RICH4_AUDIO_EVENT_CARD_ANIMATION, 1,
            session->presentation.monster_card.sound_resource);
        session->presentation.monster_card.sound_resource = 0;
    }
    current = session->state.current_player;
    moving_player = session->state.active && current >= 0 &&
        current < session->state.player_count
            ? &session->state.players[current] : NULL;
    moving = moving_player != NULL &&
        session->presentation.actors[current].move_active;
    method = rich4_audio_movement_sound_method(moving_player, movement_map);
    if (moving && movement_map != NULL) {
        GamePlayerState destination = *moving_player;
        /* 0x40C1BE changes the logical node before 0x40B93B chooses audio. */
        destination.node = session->presentation.actors[current].move_to_node;
        method = rich4_audio_movement_sound_method(&destination, movement_map);
    }
    bomb_moving = 0;
    if (session->presentation.hotel_entry.active) {
        moving = session->presentation.hotel_entry.phase <= 2;
        bomb_moving = moving;
    }
    if (session->presentation.confinement_exit.active &&
        session->presentation.confinement_exit.kind == GAME_CONFINEMENT_HOTEL) {
        moving = moving || session->presentation.hotel_entry.wait_ticks == 9;
        bomb_moving = moving;
    }
    if (moving_player != NULL &&
        session->flow.turn_state == GAME_TURN_STATE_MOVE) {
        /* 0x40D7C4: EA2 stays moving across node boundaries. move_active
         * describes only one edge and drops before the arrival tick. */
        moving = session->flow.turn_wait == RICH4_MOVE_ARRIVAL ||
            session->flow.turn_wait == RICH4_MOVE_AFTER_BANK ||
            session->flow.turn_wait == RICH4_MOVE_AFTER_BEGGAR ||
            (session->flow.turn_wait == 0 && session->flow.steps_remaining > 0);
        /* 0x41BD08 stops movement at a roadblock; 0x40D90E stops the
         * attached bomb on the next movement tick. Transfer/explosion
         * clear the attachment immediately (0x41B7E7 / 0x41B6D8). */
        bomb_moving = moving || session->flow.turn_wait == RICH4_MOVE_FORCED_STOP;
    }
    if (session->presentation.special_actor.active) {
        const GameSpecialActorPresentationState *special =
            &session->presentation.special_actor;
        /* 0x40DEB7: actors 4..7 use the same walking/water loops. */
        moving = special->steps_remaining > 0 && special->action_batch.count == 0;
        method = special->resource_part == 2 ? RICH4_MOVEMENT_SOUND_WATER :
            RICH4_MOVEMENT_SOUND_WALK;
        bomb_moving = 0;
    }
    rich4_audio_event_emit(
        audio_events, RICH4_AUDIO_EVENT_MOVEMENT, moving, method);
    rich4_audio_event_emit(audio_events, RICH4_AUDIO_EVENT_ATTACHED_BOMB,
        bomb_moving && moving_player->attached_timed_bomb_slot >= 0, 32);
}
