#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/bank_flow.h"
#include "core/game_command.h"
#include "core/rng.h"
#include "data/character_speech.h"

static Rich4AudioEvent pop_audio(Rich4AudioEventQueue *events)
{
    Rich4AudioEvent event;
    memset(&event, 0, sizeof(event));
    assert(rich4_audio_event_pop(events, &event));
    return event;
}

int main(void)
{
    GameSession session;
    Rich4AudioEventQueue audio_events;
    Rich4AudioEvent audio_event;

    memset(&session, 0, sizeof(session));
    rich4_audio_event_queue_reset(&audio_events);
    session.presentation.route_interaction.active = 1;
    session.presentation.route_interaction.accept.type =
        GAME_COMMAND_BANK_APPLY_LOAN;
    rich4_bank_flow_begin_interaction(
        &session, GAME_COMMAND_BANK_APPLY_LOAN, NULL, 1000, 0,
        &audio_events);
    assert(session.presentation.bank_interaction.stage ==
           RICH4_BANK_PRESENT_GREETING_2);
    assert(rich4_bank_flow_update(&session, 1050, 0, &audio_events));
    assert(session.presentation.bank_interaction.stage ==
           RICH4_BANK_PRESENT_READY);

    /* The original WM_TIMER advances only one 50ms step when messages are
     * overdue.  A 100ms frame must not replay two blink steps. */
    session.presentation.bank_interaction.face_chunk = -1;
    session.presentation.bank_interaction.eye_mode = 0;
    session.presentation.bank_interaction.anim_tick = 1050;
    session.state.ai_rng_state = 0;
    assert(rich4_bank_flow_update(&session, 1150, 0, &audio_events));
    assert(session.presentation.bank_interaction.face_chunk == -1);
    assert(rich4_bank_flow_update(&session, 1200, 0, &audio_events));
    assert(session.presentation.bank_interaction.face_chunk == 7);

    assert(rich4_bank_flow_begin_amount_prompt(&session, 0, 1250));
    assert(session.presentation.bank_interaction.card_x == 640);
    assert(session.presentation.bank_interaction.card_velocity == -40);
    assert(!session.presentation.bank_interaction.amount_entry);
    assert(session.presentation.bank_interaction.speech != NULL &&
           session.presentation.bank_interaction.speech->speaking_resource == 78);
    assert(rich4_bank_flow_update(&session, 1300, 0, &audio_events));
    audio_event = pop_audio(&audio_events);
    assert(audio_event.type == RICH4_AUDIO_EVENT_SPEECH_START &&
           audio_event.value == 78);
    assert(session.presentation.bank_interaction.card_x == 600);
    assert(rich4_bank_flow_update(&session, 1350, 0, &audio_events));
    assert(rich4_bank_flow_update(&session, 1400, 0, &audio_events));
    assert(rich4_bank_flow_update(&session, 1450, 0, &audio_events));
    assert(rich4_bank_flow_update(&session, 1500, 0, &audio_events));
    assert(session.presentation.bank_interaction.card_x == 440);
    assert(!session.presentation.bank_interaction.amount_entry);
    assert(rich4_bank_flow_update(&session, 3250, 1, &audio_events));
    assert(!session.presentation.bank_interaction.amount_entry);
    assert(rich4_bank_flow_update(&session, 3300, 0, &audio_events));
    assert(session.presentation.bank_interaction.amount_entry);
    assert(session.presentation.bank_interaction.stage ==
           RICH4_BANK_PRESENT_LOAN_AMOUNT);

    assert(rich4_bank_flow_begin_result(
        &session, GAME_COMMAND_BANK_APPLY_LOAN, 3350));
    assert(!session.presentation.bank_interaction.amount_entry);
    assert(session.presentation.bank_interaction.speech->speaking_resource == 79);
    assert(rich4_bank_flow_update(&session, 3400, 0, &audio_events));
    assert(pop_audio(&audio_events).value == 79);
    assert(rich4_bank_flow_update(&session, 5350, 0, &audio_events));
    assert(session.presentation.bank_interaction.stage ==
           RICH4_BANK_PRESENT_LOAN_RESULT_2);
    assert(session.presentation.bank_interaction.speech->speaking_resource == 80);
    assert(pop_audio(&audio_events).value == 80);
    assert(rich4_bank_flow_update(&session, 7350, 0, &audio_events));
    assert(session.presentation.bank_interaction.stage ==
           RICH4_BANK_PRESENT_RETRACT);
    assert(session.presentation.bank_interaction.card_velocity == 40);
    assert(rich4_bank_flow_update(&session, 7400, 0, &audio_events));
    assert(rich4_bank_flow_update(&session, 7450, 0, &audio_events));
    assert(rich4_bank_flow_update(&session, 7500, 0, &audio_events));
    assert(rich4_bank_flow_update(&session, 7550, 0, &audio_events));
    assert(rich4_bank_flow_update(&session, 7600, 0, &audio_events));
    assert(session.presentation.bank_interaction.card_x == 640);
    assert(session.presentation.route_interaction.active);
    assert(!rich4_bank_flow_update(&session, 7650, 0, &audio_events));
    assert(!session.presentation.route_interaction.active);

    session.presentation.bank_settlement.active = 1;
    session.presentation.bank_settlement.tick = 8000;
    session.presentation.bank_settlement.until = 8300;
    assert(rich4_bank_flow_update(&session, 8200, 0, &audio_events));
    assert(session.presentation.bank_settlement.frame == 2);
    assert(!rich4_bank_flow_update(&session, 8300, 0, &audio_events));
    assert(!session.presentation.bank_settlement.active);

    session.presentation.bank_loan_due.active = 1;
    session.presentation.bank_loan_due.until = 9000;
    assert(rich4_bank_flow_update(&session, 8999, 0, &audio_events));
    assert(!rich4_bank_flow_update(&session, 9000, 0, &audio_events));
    assert(!session.presentation.bank_loan_due.active);

    puts("app bank flow: ok");
    return 0;
}
