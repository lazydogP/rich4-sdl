#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "app/insurance_flow.h"
#include "app/game_presentation.h"
#include "app/special_presentation_flow.h"
#include "app/tool_animation_flow.h"
#include "core/card.h"
#include "core/confinement.h"
#include "core/facility.h"
#include "core/game_event_payload.h"
#include "core/insurance.h"
#include "core/special_event.h"
#include "core/status_countdown.h"
#include "core/tool.h"

static GameSession session;
static GameState before;

static Rich4PresentationMode current_mode(void)
{
    Rich4PresentationActivity activity = rich4_presentation_activity_from_state(
        &session.presentation, &session.state);
    return rich4_presentation_mode_resolve(&activity);
}

static void setup(void)
{
    int i;
    memset(&session, 0, sizeof(session));
    session.state.active = 1;
    session.state.player_count = 3;
    session.state.price_index = 2;
    session.state.prison_node = 10;
    session.state.hospital_node = 20;
    session.state.elapsed_days = 100;
    for (i = 0; i < 3; i++) {
        session.state.players[i].play_state = 1;
        session.state.players[i].cash = 500000;
        session.state.players[i].bank = 12345;
        session.state.players[i].node = i + 1;
        session.state.players[i].days_assurance = 1;
        session.state.players[i].monthly_income = 17;
        session.state.players[i].attached_timed_bomb_slot = -1;
    }
    session.state.company_count = 3;
    session.state.company_type[0] = 1;
    session.state.company_type[1] = session.state.company_type[2] = 4;
    session.state.company_stock_index[0] = 0;
    session.state.company_stock_index[1] = -1;
    session.state.company_stock_index[2] = 6;
    session.state.company_period_business[1] = 100;
    session.state.company_total_business[1] = 200;
    session.state.company_period_business[2] = 300;
    session.state.company_total_business[2] = 400;
}

static void prepare_fortune(int id, int value)
{
    session.state.special_event_family = GAME_SPECIAL_EVENT_FORTUNE;
    session.state.special_event_actor = 0;
    session.state.special_event_resolved_id = id;
    session.state.special_event_logical_id = id;
    session.state.special_event_prepared_value = value;
    session.state.special_event_phase = GAME_SPECIAL_PHASE_PREPARED;
}

static int capture_claims(GameEventQueue *events, uint32_t now)
{
    GameEvent e;
    int count = 0;
    while (rich4_game_event_queue_pop(events, &e))
        if (e.type == GAME_EVENT_INSURANCE_CLAIM_PRESENTATION_REQUESTED) {
            count++;
            rich4_insurance_flow_enqueue_notice(&session, &e, now);
        }
    return count;
}

/* 0x44C0E8 and every shared incoming jump to 0x44CEC2/0x44D172. */
static void monetary_fortune_matrix(void)
{
    static const int rows[][2] = {
        {2,10000}, {14,3000}, {15,3000}, {16,3000}, {17,6000},
        {18,600}, {19,1500}, {23,1000}, {24,2000}, {26,8000}, {30,5000}
    };
    static const int coverage[] = {0,1,0x80};
    int row, price, insured, defense, deferred;
    for (row = 0; row < 11; row++)
    for (price = 1; price <= 3; price += 2)
    for (insured = 0; insured < 3; insured++)
    for (defense = 0; defense <= 2; defense++)
    for (deferred = 0; deferred <= 1; deferred++) {
        GameEventQueue q;
        GameEvent e;
        int claims = 0, amount, expected;
        setup();
        session.state.price_index = price;
        session.state.players[0].days_assurance = coverage[insured];
        session.state.players[0].loan = 700;
        session.state.players[0].loan_due_date = 222;
        amount = defense == GAME_SPECIAL_DEFENSE_BLOCKED ? 0 :
            rows[row][1] * price * (defense == GAME_SPECIAL_DEFENSE_DOUBLED ? 2 : 1);
        prepare_fortune(rows[row][0], rows[row][1] * price);
        rich4_game_event_queue_reset(&q);
        assert((deferred ? rich4_special_event_commit_deferred_confinement :
                rich4_special_event_commit)(&session.state,
                (GameSpecialDefenseOutcome)defense, &q) == GAME_SPECIAL_EVENT_APPLIED);
        expected = coverage[insured] && amount ? amount : 0;
        assert(session.state.players[0].loan == 700 + (row == 0 ? amount : 0));
        assert(session.state.players[0].loan_due_date == 222);
        assert(session.state.players[0].cash == 500000 - (row == 0 ? 0 : amount) +
            (deferred ? 0 : expected));
        while (rich4_game_event_queue_pop(&q, &e))
            if (e.type == GAME_EVENT_INSURANCE_CLAIM_PRESENTATION_REQUESTED) {
                claims++;
                assert(rich4_game_event_insurance_company(&e) == 1);
                assert(rich4_game_event_insurance_cause(&e) == GAME_INSURANCE_CLAIM_FORTUNE);
                assert(rich4_game_event_insurance_amount(&e) == expected);
                if (deferred) rich4_insurance_flow_enqueue_notice(&session, &e, 1000);
            }
        assert(claims == (expected != 0));
        if (expected && deferred) {
            assert(rich4_insurance_flow_update(&session, 1000));
            assert(rich4_insurance_flow_update(&session, 2999));
            assert(session.state.players[0].monthly_income == 17);
            assert(!rich4_insurance_flow_update(&session, 3000));
            assert(!rich4_insurance_flow_update(&session, 3001));
        }
        assert(session.state.players[0].cash == 500000 - (row == 0 ? 0 : amount) + expected);
        assert(session.state.players[0].bank == 12345);
        assert(session.state.players[0].monthly_income == 17 + expected);
        assert(session.state.players[0].days_assurance == coverage[insured]);
        assert(session.state.players[0].total_confinement_days == 0);
        assert(session.state.company_period_business[1] == 100u - (uint32_t)expected);
        assert(session.state.company_total_business[1] == 200u - (uint32_t)expected);
        assert(session.state.company_total_business[2] == 400u);
    }
}

static void disappearing_and_confinement(void)
{
    GameEventQueue q;
    int kind;
    for (kind = GAME_CONFINEMENT_PRISON; kind <= GAME_CONFINEMENT_HOSPITAL; kind++) {
        setup();
        session.state.players[0].total_confinement_days = 254;
        rich4_game_event_queue_reset(&q);
        assert(rich4_confinement_enter(&session.state, 0, kind, 3, &q) == GAME_CONFINEMENT_APPLIED);
        assert(session.state.players[0].cash == 512000);
        assert(session.state.players[0].total_confinement_days == 1);
        rich4_game_event_queue_reset(&q);
        assert(rich4_confinement_enter(&session.state, 0, kind, 5, &q) == GAME_CONFINEMENT_APPLIED);
        assert(session.state.players[0].cash == 532000);
        assert(session.state.players[0].total_confinement_days == 6);
        assert((kind == GAME_CONFINEMENT_PRISON ? session.state.players[0].days_in_prison :
            session.state.players[0].days_in_hospital) == 8);
    }
    /* 0x80 still selects the extension path; all byte fields wrap. */
    setup();
    session.state.players[0].days_in_prison = 0x80;
    session.state.players[0].node = 31;
    rich4_game_event_queue_reset(&q);
    assert(rich4_confinement_enter(&session.state, 0, GAME_CONFINEMENT_PRISON, 4, &q) == GAME_CONFINEMENT_APPLIED);
    assert(session.state.players[0].days_in_prison == 4 && session.state.players[0].node == 31);
    session.state.players[0].days_in_prison = 126;
    assert(rich4_confinement_enter(&session.state, 0, GAME_CONFINEMENT_PRISON, 5, &q) == GAME_CONFINEMENT_APPLIED);
    assert(session.state.players[0].days_in_prison == 3);

    for (kind = 0; kind <= 1; kind++) {
        setup();
        session.state.players[0].traffic_method = 2;
        session.state.players[0].ndices = 3;
        session.state.players[0].days_in_hospital = 5;
        rich4_game_event_queue_reset(&q);
        assert(rich4_confinement_disappear(&session.state, 0, 3, kind, 0, &q) == GAME_CONFINEMENT_APPLIED);
        assert(session.state.players[0].cash == 512000);
        assert(session.state.players[0].days_in_hospital == 0);
        assert(session.state.players[0].days_disappearing == 3 + 64 * kind);
        assert(session.state.players[0].traffic_method == 2 && session.state.players[0].ndices == 3);
        rich4_game_event_queue_reset(&q);
        assert(rich4_confinement_disappear(&session.state, 0, 3, 1 - kind, 0, &q) == GAME_CONFINEMENT_APPLIED);
        assert(session.state.players[0].days_disappearing == 6 + 64 * (1 - kind));
        assert(session.state.players[0].cash == 512000 && session.state.players[0].total_confinement_days == 3);
        assert(capture_claims(&q, 1000) == 0);
    }
    /* Specials never receive cash or insurance notices. */
    setup();
    rich4_game_event_queue_reset(&q);
    assert(rich4_confinement_enter(&session.state, 4, GAME_CONFINEMENT_HOSPITAL, 3, &q) == GAME_CONFINEMENT_APPLIED);
    assert(capture_claims(&q, 1000) == 0);
}

static void beneficiaries_and_non_claims(void)
{
    GameEventQueue q;
    GameCompanyLandingResult company;
    int id;
    setup();
    session.state.players[0].card_inventory[0] = 19;
    session.state.players[0].days_assurance = 0;
    rich4_game_event_queue_reset(&q);
    assert(rich4_card_resolve_attack(&session.state, 1, 0, GAME_CARD_ATTACK_FRAME, 2, &q));
    assert(session.state.players[2].days_in_prison == 5 && session.state.players[2].cash == 520000);
    assert(session.state.players[0].cash == 500000 && session.state.players[0].days_in_prison == 0);
    setup();
    session.state.players[0].card_inventory[0] = 19;
    rich4_game_event_queue_reset(&q);
    assert(rich4_card_resolve_attack(&session.state, 1, 0, GAME_CARD_ATTACK_FRAME, 1, &q));
    assert(session.state.players[1].days_in_prison == 4 && session.state.players[1].cash == 516000);
    setup();
    session.state.players[0].card_inventory[0] = 18;
    rich4_game_event_queue_reset(&q);
    assert(rich4_card_resolve_attack(&session.state, 1, 0, GAME_CARD_ATTACK_FRAME, -1, &q));
    assert(session.state.players[0].cash == 520000 && session.state.players[1].cash == 520000);

    /* Aviation assigns travel and its claim to the final god substitute. */
    setup();
    session.state.company_base_fee[0] = 100;
    session.state.stock_company_owner[0] = 1;
    session.state.players[2].god_info = session.state.players[2].god_type = 14;
    rich4_game_event_queue_reset(&q);
    assert(rich4_company_resolve_landing(&session.state, 0, 0, 3, 1, 0, -1,
        &company, &q) == GAME_PAYMENT_COMMITTED);
    assert(company.payer == 2 && session.state.players[2].days_disappearing == 3);
    assert(session.state.players[0].days_disappearing == 0);
    assert(capture_claims(&q, 1000) == 1);
    assert(session.presentation.insurance.queue[0].player == 2);
    assert(rich4_insurance_flow_update(&session, 1000));
    assert(!rich4_insurance_flow_update(&session, 3000));
    assert(session.state.players[2].cash == 511400);
    rich4_game_event_queue_reset(&q);
    assert(rich4_company_resolve_landing(&session.state, 0, 0, 2, 1, 0, -1,
        &company, &q) == GAME_PAYMENT_COMMITTED);
    assert(session.state.players[2].days_disappearing == 5 && capture_claims(&q, 4000) == 0);

    for (id = 0; id < 2; id++) {
        setup();
        session.state.players[0].cash = 1;
        session.state.players[0].bank = 0;
        prepare_fortune(23, 2000);
        rich4_game_event_queue_reset(&q);
        assert((id ? rich4_special_event_commit_deferred_confinement : rich4_special_event_commit)(
            &session.state, GAME_SPECIAL_DEFENSE_NONE, &q) == GAME_SPECIAL_EVENT_BANKRUPTCY);
        assert(capture_claims(&q, 1000) == 0);
        assert(session.state.players[0].cash == 0 && session.state.company_total_business[1] == 200);
    }
    for (id = 10; id <= 11; id++) {
        setup();
        session.state.players[0].traffic_method = id == 10 ? 1 : 2;
        prepare_fortune(id, 1);
        rich4_game_event_queue_reset(&q);
        assert(rich4_special_event_commit(&session.state, GAME_SPECIAL_DEFENSE_NONE, &q) == GAME_SPECIAL_EVENT_APPLIED);
        assert(capture_claims(&q, 1000) == 0);
        assert(session.state.players[0].cash == 500000);
    }
}

static void timing_and_policy_edges(void)
{
    GameEventQueue q;
    GameInsuranceClaimResult claim;
    StatusTickResult tick;
    int i;
    setup();
    before = session.state;
    rich4_game_event_queue_reset(&q);
    assert(rich4_insurance_request_claim_for_days(&session.state, 0, 5,
        GAME_INSURANCE_CLAIM_HOSPITAL, &q));
    assert(memcmp(&before, &session.state, sizeof(before)) == 0);
    assert(capture_claims(&q, 1000) == 1);
    assert(rich4_insurance_flow_update(&session, 1000));
    assert(rich4_insurance_flow_skip(&session, 1100));
    assert(!rich4_insurance_flow_update(&session, 1100));
    assert(session.state.players[0].cash == 520000 && session.state.players[0].total_confinement_days == 5);
    assert(session.state.players[0].days_assurance == 1);
    assert(!rich4_insurance_flow_update(&session, 3000));

    setup();
    session.presentation.special_event.active = 1;
    rich4_game_event_queue_reset(&q);
    assert(rich4_insurance_request_claim(&session.state, 0, 6000,
        GAME_INSURANCE_CLAIM_FORTUNE, 0, &q));
    assert(capture_claims(&q, 1000) == 1);
    assert(current_mode() != RICH4_PRESENTATION_INSURANCE);
    assert(!rich4_insurance_flow_update(&session, 100000));
    assert(!rich4_insurance_flow_skip(&session, 100000));
    assert(rich4_insurance_flow_resume_special_claim(&session, 100000));
    assert(current_mode() == RICH4_PRESENTATION_INSURANCE);
    assert(!rich4_insurance_flow_update(&session, 102000));
    assert(session.state.players[0].cash == 506000 && session.state.players[0].total_confinement_days == 0);

    setup();
    session.state.players[0].days_assurance = 0;
    memset(&claim, 0xff, sizeof(claim));
    rich4_game_event_queue_reset(&q);
    assert(rich4_insurance_claim_for_days(&session.state, 0, 3,
        GAME_INSURANCE_CLAIM_HOSPITAL, &claim, &q));
    assert(!claim.applied && claim.company == -1 && q.count == 0);
    setup();
    before = session.state;
    rich4_game_event_queue_reset(&q);
    for (i = 0; i < GAME_EVENT_QUEUE_CAPACITY; i++) {
        GameEvent e = rich4_game_event_insurance_fee(0, 1, 0, 0, 1500, 0);
        assert(rich4_game_event_queue_push(&q, &e));
    }
    assert(!rich4_insurance_request_claim(&session.state, 0, 1, GAME_INSURANCE_CLAIM_FORTUNE, 0, &q));
    assert(memcmp(&before, &session.state, sizeof(before)) == 0);
    session.state.players[0].days_in_hospital = 5;
    rich4_status_tick_player(&session.state.players[0], &tick);
    assert(session.state.players[0].days_assurance == 0x80);
    rich4_game_event_queue_reset(&q);
    assert(rich4_insurance_claim(&session.state, 0, 10, GAME_INSURANCE_CLAIM_FORTUNE, &claim, &q));
    assert(claim.applied);
}

/* Runtime card admission traverses reaction, FLC, sync and final speech.
 * Only then is 0x44BA63's notice emitted, and only its return credits cash. */
static void runtime_card_admission(void)
{
    GameEventQueue q;
    GameEvent e;
    GameCommand command;
    int i, notices = 0;
    setup();
    session.state.players[0].card_inventory[0] = 17;
    command = rich4_game_domain_command(GAME_COMMAND_USE_CARD,
        GAME_COMMAND_SOURCE_HUMAN, 0, rich4_game_domain_ref(GAME_DOMAIN_CARD, 16),
        rich4_game_domain_ref(GAME_DOMAIN_PLAYER, 1), 0);
    rich4_game_event_queue_reset(&q);
    assert(rich4_card_apply_command_deferred_confinement(&session.state, &command, &q) == GAME_CARD_USE_APPLIED);
    assert(session.state.players[1].days_in_prison == 0 && session.state.players[1].cash == 500000);
    while (rich4_game_event_queue_pop(&q, &e)) {
        assert(e.type != GAME_EVENT_INSURANCE_CLAIM_PRESENTATION_REQUESTED);
        if (e.type == GAME_EVENT_TEXT_REQUESTED) {
            Rich4GameEventFields fields = rich4_game_event_text_requested_fields(&e);
            assert(e.player == 1 && fields.value == GAME_CONFINEMENT_PRISON && fields.value2 == 5);
            assert(rich4_confinement_presentation_enqueue(&session, e.player, fields.value, fields.value2, 1000));
        }
    }
    for (i = 0; i < 16 && session.presentation.tool_animation_count; i++) {
        Rich4ToolAnimationCompletion completed;
        session.presentation.speech.active = 0; /* voice has ended */
        rich4_tool_animation_update(&session, 0, 1, 1001 + (uint32_t)i,
            NULL, &completed);
        if (completed.sync_player >= 0)
            rich4_tool_animation_actor_synced(&session, completed.sync_player);
        while (rich4_game_event_queue_pop(&completed.events, &e))
            if (e.type == GAME_EVENT_INSURANCE_CLAIM_PRESENTATION_REQUESTED) {
                notices++;
                rich4_insurance_flow_enqueue_confinement_claim(&session, &e, 1010);
            }
        assert(session.state.players[1].cash == 500000 && session.state.players[1].monthly_income == 17);
    }
    assert(session.presentation.tool_animation_count == 0 && notices == 1);
    assert(session.state.players[1].days_in_prison == 5 && session.state.players[1].total_confinement_days == 0);
    assert(rich4_insurance_flow_update(&session, 2000));
    assert(rich4_insurance_flow_update(&session, 3999));
    assert(!rich4_insurance_flow_update(&session, 4000));
    assert(session.state.players[1].cash == 520000 && session.state.players[1].total_confinement_days == 5);
}

static void runtime_missile_request(void)
{
    static const uint8_t legal[4] = {0,1,1,1};
    static const int16_t x[4] = {0,100,110,600};
    static const int16_t y[4] = {0,100,110,600};
    static const uint16_t next[4][4] = {{0}};
    GameCommand command;
    GameEventQueue q;
    GameEvent e;
    int tool, admissions;
    for (tool = 6; tool <= 12; tool += 6) {
        setup();
        session.state.hospital_node = 3;
        session.state.players[0].node = 3;
        session.state.players[0].tool_inventory[tool] = 1;
        assert(rich4_tool_set_route_targets(&session.state, 4, legal, sizeof(legal)));
        assert(rich4_tool_set_map_geometry(&session.state, 4, x, y, next,
            0, NULL, NULL, 0, NULL, NULL));
        command = rich4_game_domain_command(GAME_COMMAND_USE_TOOL,
            GAME_COMMAND_SOURCE_HUMAN, 0, rich4_game_domain_ref(GAME_DOMAIN_TOOL, tool),
            rich4_game_domain_ref(GAME_DOMAIN_ROUTE_NODE, 2), 0);
        rich4_game_event_queue_reset(&q);
        assert(rich4_tool_apply_command_deferred_confinement(&session.state, &command, &q) == GAME_TOOL_USE_APPLIED);
        assert(session.state.players[1].node == 2 && session.state.players[1].days_in_hospital == 0);
        assert(session.state.players[1].cash == 500000 && session.state.players[1].total_confinement_days == 0);
        admissions = 0;
        while (rich4_game_event_queue_pop(&q, &e)) {
            assert(e.type != GAME_EVENT_INSURANCE_CLAIM_PRESENTATION_REQUESTED);
            if (e.type == GAME_EVENT_TEXT_REQUESTED) {
                Rich4GameEventFields fields = rich4_game_event_text_requested_fields(&e);
                assert(e.player == 1 && fields.value == GAME_CONFINEMENT_HOSPITAL && fields.value2 == 3);
                admissions++;
            }
        }
        assert(admissions == 1);
    }
}

int main(void)
{
    monetary_fortune_matrix();
    disappearing_and_confinement();
    beneficiaries_and_non_claims();
    timing_and_policy_edges();
    runtime_card_admission();
    runtime_missile_request();
    puts("insurance original-alignment matrix: ok");
    return 0;
}
