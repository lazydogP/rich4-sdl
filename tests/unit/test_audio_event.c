#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/audio_event.h"
#include "app/game_session.h"
#include "app/turn_movement_flow.h"
#include "core/game_types.h"
#include "data/map_data.h"

int main(void)
{
    Rich4AudioEventQueue queue;
    Rich4AudioEvent event;
    GamePlayerState player;
    MapRouteNode routes[2];
    MapData map;
    size_t i;
    memset(&player, 0, sizeof(player));
    memset(routes, 0, sizeof(routes));
    memset(&map, 0, sizeof(map));
    player.node = 1;
    player.traffic_method = 3;
    map.routes = routes;
    map.route_count = 2;
    assert(rich4_audio_movement_sound_method(&player, &map) ==
           RICH4_MOVEMENT_SOUND_ENGINEERING_VEHICLE);
    routes[1].flags = 0x80000000u;
    assert(rich4_audio_movement_sound_method(&player, &map) ==
           RICH4_MOVEMENT_SOUND_WATER);
    assert(rich4_audio_movement_sound_method(NULL, &map) ==
           RICH4_MOVEMENT_SOUND_WALK);
    {
        GameSession session;
        memset(&session, 0, sizeof(session));
        session.state.active = 1;
        session.state.player_count = 1;
        session.state.current_player = 0;
        session.presentation.dice.sound_pending = 1;
        session.presentation.property.sound_pending = 3;
        session.presentation.god_detachment.sound_pending = 1;
        session.presentation.god_acquisition.sound_pending = 1;
        session.presentation.god_acquisition.god_type = 15;
        session.presentation.monster_card.sound_resource = 97;
        session.presentation.actors[0].move_active = 1;
        rich4_audio_event_queue_init(&queue);
        rich4_app_collect_session_audio_events(&session, &map, &queue);
        assert(session.presentation.dice.sound_pending == 0);
        assert(session.presentation.property.sound_pending == 0);
        assert(session.presentation.god_acquisition.sound_pending == 0);
        assert(session.presentation.monster_card.sound_resource == 0);
        assert(session.presentation.god_detachment.sound_pending == 0);
        assert(queue.count == 7);
        assert(rich4_audio_event_pop(&queue, &event) &&
               event.type == RICH4_AUDIO_EVENT_DICE);
        assert(rich4_audio_event_pop(&queue, &event) &&
               event.type == RICH4_AUDIO_EVENT_PROPERTY_TOLL);
        assert(rich4_audio_event_pop(&queue, &event) &&
               event.type == RICH4_AUDIO_EVENT_GOD_DEPARTURE && event.value == 54);
        assert(rich4_audio_event_pop(&queue, &event) &&
               event.type == RICH4_AUDIO_EVENT_GOD_EFFECT &&
               event.value == 15);
        assert(rich4_audio_event_pop(&queue, &event) &&
               event.type == RICH4_AUDIO_EVENT_CARD_ANIMATION &&
               event.value == 97);
        assert(rich4_audio_event_pop(&queue, &event) &&
               event.type == RICH4_AUDIO_EVENT_MOVEMENT && event.active);
        assert(rich4_audio_event_pop(&queue, &event) &&
               event.type == RICH4_AUDIO_EVENT_ATTACHED_BOMB && !event.active);
        rich4_app_collect_session_audio_events(&session, &map, &queue);
        assert(queue.count == 2); /* One-shot god sounds must not repeat. */
        assert(rich4_audio_event_pop(&queue, &event) &&
               event.type == RICH4_AUDIO_EVENT_MOVEMENT);
        assert(rich4_audio_event_pop(&queue, &event) &&
               event.type == RICH4_AUDIO_EVENT_ATTACHED_BOMB);
        session.flow.turn_state = GAME_TURN_STATE_MOVE;
        session.flow.steps_remaining = 2;
        session.state.players[0].attached_timed_bomb_slot = 0;
        session.presentation.actors[0].move_active = 0;
        session.flow.turn_wait = RICH4_MOVE_ARRIVAL;
        rich4_app_collect_session_audio_events(&session, &map, &queue);
        assert(rich4_audio_event_pop(&queue, &event) && event.active);
        assert(rich4_audio_event_pop(&queue, &event) && event.active);
        /* Transfer stops only the bomb, even across an edge boundary. */
        session.state.players[0].attached_timed_bomb_slot = -1;
        rich4_app_collect_session_audio_events(&session, &map, &queue);
        assert(rich4_audio_event_pop(&queue, &event) && event.active);
        assert(rich4_audio_event_pop(&queue, &event) && !event.active);
        /* Roadblock: movement stops first, bomb stops on the next tick. */
        session.state.players[0].attached_timed_bomb_slot = 0;
        session.flow.steps_remaining = 0;
        session.flow.turn_wait = RICH4_MOVE_FORCED_STOP;
        rich4_app_collect_session_audio_events(&session, &map, &queue);
        assert(rich4_audio_event_pop(&queue, &event) && !event.active);
        assert(rich4_audio_event_pop(&queue, &event) && event.active);
        session.flow.turn_wait = 5;
        rich4_app_collect_session_audio_events(&session, &map, &queue);
        assert(rich4_audio_event_pop(&queue, &event) && !event.active);
        assert(rich4_audio_event_pop(&queue, &event) && !event.active);
    }
    rich4_audio_event_queue_init(&queue);
    assert(rich4_audio_event_emit(&queue, RICH4_AUDIO_EVENT_DICE, 1, 0));
    assert(rich4_audio_event_emit(&queue, RICH4_AUDIO_EVENT_MOVEMENT, 1, 3));
    assert(rich4_audio_event_emit(&queue, RICH4_AUDIO_EVENT_PROPERTY_PURCHASE, 1, 0));
    assert(rich4_audio_event_emit(&queue, RICH4_AUDIO_EVENT_PROPERTY_TOLL, 1, 0));
    assert(rich4_audio_event_emit(&queue, RICH4_AUDIO_EVENT_PROPERTY_MAX_LEVEL, 1, 0));
    assert(rich4_audio_event_pop(&queue, &event));
    assert(event.type == RICH4_AUDIO_EVENT_DICE);
    assert(rich4_audio_event_pop(&queue, &event));
    assert(event.type == RICH4_AUDIO_EVENT_MOVEMENT && event.active && event.value == 3);
    assert(rich4_audio_event_pop(&queue, &event));
    assert(event.type == RICH4_AUDIO_EVENT_PROPERTY_PURCHASE);
    assert(rich4_audio_event_pop(&queue, &event));
    assert(event.type == RICH4_AUDIO_EVENT_PROPERTY_TOLL);
    assert(rich4_audio_event_pop(&queue, &event));
    assert(event.type == RICH4_AUDIO_EVENT_PROPERTY_MAX_LEVEL);
    assert(!rich4_audio_event_pop(&queue, &event));
    {
        GameSession session;
        Rich4AudioEventQueue *borrowed;
        memset(&session, 0, sizeof(session));
        borrowed = rich4_app_session_audio_borrow(&session);
        assert(borrowed == &session.audio_events);
        assert(rich4_audio_event_emit(
            borrowed, RICH4_AUDIO_EVENT_SPEECH_START, 1, 37));
        rich4_audio_event_queue_reset(borrowed);
        assert(session.audio_events.borrow_depth == 1);
        assert(session.audio_events.count == 0);
        assert(rich4_app_session_audio_release(&session) ==
               &session.audio_events);
    }
    for (i = 0; i < RICH4_AUDIO_EVENT_CAPACITY; i++)
        assert(rich4_audio_event_emit(&queue, RICH4_AUDIO_EVENT_DOOR, 1, (int)i));
    assert(!rich4_audio_event_emit(&queue, RICH4_AUDIO_EVENT_JUMP, 1, 0));
    assert(queue.dropped == 1);
    for (i = 0; i < RICH4_AUDIO_EVENT_CAPACITY; i++) {
        assert(rich4_audio_event_pop(&queue, &event));
        assert(event.value == (int)i);
    }
    assert(!rich4_audio_event_emit(&queue, RICH4_AUDIO_EVENT_NONE, 0, 0));
    puts("audio event queue: ok");
    return 0;
}
