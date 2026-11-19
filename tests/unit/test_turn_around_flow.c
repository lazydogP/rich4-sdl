#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/audio_event.h"
#include "app/command_event_flow.h"
#include "core/game_event_payload.h"

static void apply_card_turn(GameSession *session, GameDomainRef target)
{
    GameCommand command = rich4_game_command_use_card_on_target(
        GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(GAME_DOMAIN_CARD, 5),
        target, 0, 0, 0);
    Rich4GameEventFields fields = {0};
    GameEvent event;

    fields.player = 0;
    fields.subject_ref = rich4_game_domain_ref(GAME_DOMAIN_CARD, 5);
    fields.target_ref = target;
    event = rich4_game_event_status_changed_from_fields(fields);
    assert(rich4_app_apply_command_event(
               session, &command, &event, 100) ==
           RICH4_COMMAND_EVENT_CONSUMED);
}

int main(void)
{
    GameSession session;
    Rich4AudioEventQueue audio;
    Rich4AudioEvent event;

    memset(&session, 0, sizeof(session));
    session.state.active = 1;
    session.state.player_count = 1;
    session.state.players[0].play_state = 1;
    session.presentation.actors[0].direction = 3;
    session.presentation.actors[0].walk_frame = 2;

    apply_card_turn(
        &session, rich4_game_domain_ref(GAME_DOMAIN_PLAYER, 0));
    assert(session.presentation.actors[0].direction == 7);
    assert(session.presentation.actors[0].walk_frame == 0);
    assert(session.presentation.turn_around_sound_pending == 1);

    rich4_audio_event_queue_init(&audio);
    rich4_app_collect_session_audio_events(&session, NULL, &audio);
    assert(session.presentation.turn_around_sound_pending == 0);
    assert(rich4_audio_event_pop(&audio, &event));
    assert(event.type == RICH4_AUDIO_EVENT_TURN_AROUND);
    assert(event.value == 56);

    session.presentation.special_actor.active = 1;
    session.presentation.special_actor.slot = 0;
    session.presentation.special_actor.direction = 1;
    session.presentation.special_actor.animation_frame = 4;
    session.state.special_player_direction[0] = 1;
    apply_card_turn(
        &session, rich4_game_domain_ref(GAME_DOMAIN_SPECIAL_PLAYER, 0));
    assert(session.presentation.special_actor.direction == 5);
    assert(session.state.special_player_direction[0] == 5);
    assert(session.presentation.special_actor.animation_frame == 0);
    assert(session.presentation.turn_around_sound_pending == 1);

    session.presentation.special_actor.active = 0;
    session.state.special_player_direction[1] = 2;
    apply_card_turn(
        &session, rich4_game_domain_ref(GAME_DOMAIN_SPECIAL_PLAYER, 1));
    assert(session.state.special_player_direction[1] == 6);
    assert(session.presentation.turn_around_sound_pending == 2);

    puts("turn-around presentation/audio flow: ok");
    return 0;
}
