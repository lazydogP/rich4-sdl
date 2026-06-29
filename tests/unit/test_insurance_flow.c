#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/insurance_flow.h"
#include "core/game_event_payload.h"
#include "core/insurance.h"

static void deferred_hospital_claim(void)
{
    GameSession session, before;
    GameEvent event;
    int i;
    memset(&session, 0, sizeof(session));
    session.state.player_count = 1;
    session.state.price_index = 1;
    session.state.players[0].play_state = 1;
    session.state.players[0].days_assurance = 5;
    session.state.company_count = 1;
    session.state.company_type[0] = 4;
    session.state.company_stock_index[0] = 0;
    event = rich4_game_event_insurance_claim(
        0, 0, 0, 6000, 2000, GAME_INSURANCE_CLAIM_HOSPITAL);
    rich4_insurance_flow_enqueue_confinement_claim(&session, &event, 1000);
    assert(session.presentation.insurance.until == 0);
    assert(session.state.players[0].cash == 0);
    assert(rich4_insurance_flow_update(&session, 1200));
    assert(rich4_insurance_flow_update(&session, 3199));
    assert(session.state.players[0].cash == 0);
    assert(session.state.players[0].total_confinement_days == 0);
    assert(!rich4_insurance_flow_update(&session, 3200));
    assert(session.state.players[0].cash == 6000);
    assert(session.state.players[0].total_confinement_days == 3);
    assert(!rich4_insurance_flow_update(&session, 3201));
    assert(session.state.players[0].cash == 6000);

    /* A five-day bomb claim stays pending until the release, then pays once. */
    session.state.price_index = 1;
    event = rich4_game_event_insurance_claim(
        0, 0, 0, 10000, 2000, GAME_INSURANCE_CLAIM_HOSPITAL);
    rich4_insurance_flow_enqueue_confinement_claim(&session, &event, 4000);
    assert(rich4_insurance_flow_update(&session, 4000));
    assert(rich4_insurance_flow_skip(&session, 4100));
    assert(!rich4_insurance_flow_update(&session, 4100));
    assert(session.state.players[0].cash == 16000);
    assert(session.state.players[0].total_confinement_days == 8);
    assert(!rich4_insurance_flow_skip(&session, 4101));

    /* Failed enqueue must not relabel the previous notice or reset its
     * deadline. App ownership also protects the bounded FIFO boundary. */
    memset(&session, 0, sizeof(session));
    for (i = 0; i < RICH4_INSURANCE_NOTICE_CAPACITY; i++)
        rich4_insurance_flow_enqueue_notice(&session, &event, 4000);
    before = session;
    rich4_insurance_flow_enqueue_confinement_claim(&session, &event, 4500);
    assert(memcmp(&session, &before, sizeof(session)) == 0);
}

int main(void)
{
    GameSession session;
    GameEvent event;
    deferred_hospital_claim();
    memset(&session, 0, sizeof(session));

    event = rich4_game_event_insurance_fee(0, 2, 3, 120, 0, 0);
    rich4_insurance_flow_enqueue_notice(&session, &event, 1000);
    assert(session.presentation.insurance.count == 1);
    assert(session.presentation.insurance.queue[0].kind ==
           GAME_INSURANCE_NOTICE_FEE);
    assert(session.presentation.insurance.queue[0].company == 2);
    assert(session.presentation.insurance.queue[0].chairman == 3);
    assert(session.presentation.insurance.until == 2500);
    assert(rich4_insurance_flow_update(&session, 2499));
    assert(!rich4_insurance_flow_update(&session, 2500));
    assert(session.presentation.insurance.count == 0);

    event = rich4_game_event_insurance_claim(0, 0, 0, 300, 500, 0);
    rich4_insurance_flow_enqueue_notice(&session, &event, 3000);
    assert(session.presentation.insurance.queue[0].duration == 500);
    assert(session.presentation.insurance.until == 3500);
    puts("app insurance flow: ok");
    return 0;
}
