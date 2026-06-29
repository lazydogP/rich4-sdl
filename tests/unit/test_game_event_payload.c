#include <assert.h>
#include <stdio.h>

#include "core/game_event_payload.h"

typedef struct TypedEventContract {
    GameEventType type;
    GameEvent (*construct)(Rich4GameEventFields fields);
    int (*validate)(const GameEvent *event);
    Rich4GameEventFields (*access)(const GameEvent *event);
} TypedEventContract;

#define RICH4_TYPED_EVENT_CONTRACT(name, type_value) \
    { type_value, rich4_game_event_##name##_from_fields, \
      rich4_game_event_##name##_is_valid, rich4_game_event_##name##_fields },
static const TypedEventContract typed_event_contracts[] = {
    RICH4_GAME_EVENT_TYPED_KINDS(RICH4_TYPED_EVENT_CONTRACT)
};
#undef RICH4_TYPED_EVENT_CONTRACT

int main(void)
{
    GameEventQueue queue;
    GameEvent event;
    GameDomainRef subject = rich4_game_domain_ref(GAME_DOMAIN_COMPANY, 3);
    GameDomainRef target = rich4_game_domain_ref(GAME_DOMAIN_PLAYER, 1);

    rich4_game_event_queue_reset(&queue);
    {
        Rich4GameEventFields fields = {
            1, 0, 88, 2000, 4, subject, target
        };
        event = rich4_game_event_insurance_claim_presentation_requested_from_fields(
            fields);
        assert(rich4_game_event_queue_push(&queue, &event));
    }
    assert(rich4_game_event_queue_pop(&queue, &event));
    assert(rich4_game_event_is_valid(&event));
    assert(event.payload.kind == RICH4_GAME_EVENT_PAYLOAD_DOMAIN);
    assert(rich4_game_event_value(&event) == 88);
    assert(rich4_game_event_value2(&event) == 2000);
    assert(rich4_game_event_flags(&event) == 4u);
    assert(rich4_game_event_subject_ref(&event).kind == GAME_DOMAIN_COMPANY);
    assert(rich4_game_event_target_ref(&event).id == 1);
    event.payload.data.domain.value = 89;
    assert(rich4_game_event_value(&event) == 89);
    assert(rich4_game_event_is_valid(&event));

    event = rich4_game_event_facility_roulette_request(
        0, rich4_game_domain_ref(GAME_DOMAIN_COMPANY, 4), 2, 70, 517, 9u);
    assert(event.type == GAME_EVENT_FACILITY_ROULETTE_REQUESTED);
    assert(rich4_game_event_facility_mode(&event) == 2);
    assert(rich4_game_event_facility_panel_resource(&event) == 70);
    assert(rich4_game_event_facility_data_resource(&event) == 517);
    assert(rich4_game_event_facility_subject(&event).id == 4);

    event = rich4_game_event_company_stock_issue_request(
        1, 3, rich4_game_domain_ref(GAME_DOMAIN_STOCK, 5), 120, 6, 10u);
    assert(event.type == GAME_EVENT_COMPANY_STOCK_ISSUE_REQUESTED);
    assert(rich4_game_event_company_issue_company(&event) == 3);
    assert(rich4_game_event_company_issue_stock(&event).id == 5);
    assert(rich4_game_event_company_issue_price(&event) == 120);
    assert(rich4_game_event_company_issue_maximum(&event) == 6);

    event = rich4_game_event_shop_visit_request(
        1, 15, 10, 11, rich4_game_domain_ref(GAME_DOMAIN_CARD, 2), 11u);
    assert(event.type == GAME_EVENT_SHOP_VISIT_REQUESTED);
    assert(rich4_game_event_shop_action(&event) == 15);
    assert(rich4_game_event_shop_gift(&event).kind == GAME_DOMAIN_CARD);

    event = rich4_game_event_magic_house_request(1, 8, 0x05u);
    assert(event.type == GAME_EVENT_MAGIC_HOUSE_REQUESTED);
    assert(rich4_game_event_magic_house_condition(&event) == 8);
    assert(rich4_game_event_magic_house_target_mask(&event) == 0x05u);

    event = rich4_game_event_special_place_request(1, 13, 536, 0, 12u);
    assert(event.type == GAME_EVENT_SPECIAL_PLACE_PRESENTATION_REQUESTED);
    assert(rich4_game_event_subject(&event) == 13);
    assert(rich4_game_event_value(&event) == 536);
    assert(rich4_game_event_value2(&event) == 0);
    assert(rich4_game_event_flags(&event) == 12u);

    event = rich4_game_event_confinement_visit_request(1,
        GAME_CONFINEMENT_PRISON, 63, 64, 13u);
    assert(event.type == GAME_EVENT_CONFINEMENT_VISIT_REQUESTED);
    assert(rich4_game_event_subject(&event) == GAME_CONFINEMENT_PRISON);
    assert(rich4_game_event_value(&event) == 63);
    assert(rich4_game_event_value2(&event) == 64);

    event = rich4_game_event_point_minigame_request(1, 6, 78, 79, 14u);
    assert(event.type == GAME_EVENT_POINT_MINIGAME_REQUESTED);
    assert(rich4_game_event_point_action(&event) == 6);
    assert(rich4_game_event_point_panel_resource(&event) == 78);
    assert(rich4_game_event_point_data_resource(&event) == 79);

    event = rich4_game_event_lottery_visit_request(1, 9, 12, 13, 14u);
    assert(event.type == GAME_EVENT_LOTTERY_VISIT_REQUESTED);
    assert(rich4_game_event_subject(&event) == 9);
    assert(rich4_game_event_value(&event) == 12);
    assert(rich4_game_event_value2(&event) == 13);
    assert(rich4_game_event_flags(&event) == 14u);

    event = rich4_game_event_bank_rejection(1, 3, 1000, 15u);
    assert(event.type == GAME_EVENT_BANK_REJECTION_PRESENTATION_REQUESTED);
    assert(rich4_game_event_subject(&event) == 14);
    assert(rich4_game_event_value(&event) == 3);
    assert(rich4_game_event_value2(&event) == 1000);

    event = rich4_game_event_bank_loan_due(1, 900, 2, 0, 0u);
    assert(event.type == GAME_EVENT_BANK_LOAN_DUE_PRESENTATION_REQUESTED);
    assert(rich4_game_event_bank_loan_principal(&event) == 900);
    assert(rich4_game_event_bank_loan_days(&event) == 2);
    assert(rich4_game_event_bank_loan_payment(&event) == 0);
    assert(rich4_game_event_bank_loan_forced(&event) == 0u);

    event = rich4_game_event_bank_settlement(1, 20, 320, 1u);
    assert(event.type == GAME_EVENT_BANK_SETTLEMENT_APPLIED);
    assert(rich4_game_event_bank_settlement_interest(&event) == 20);
    assert(rich4_game_event_bank_settlement_balance(&event) == 320);
    assert(rich4_game_event_bank_settlement_loan_outstanding(&event) == 1u);

    event = rich4_game_event_insurance_fee(1, 3, 2, 88, 1500, 5u);
    assert(event.type == GAME_EVENT_INSURANCE_FEE_PRESENTATION_REQUESTED);
    assert(rich4_game_event_insurance_company(&event) == 3);
    assert(rich4_game_event_insurance_claimant(&event) == 2);
    assert(rich4_game_event_insurance_amount(&event) == 88);
    assert(rich4_game_event_insurance_duration(&event) == 1500);
    assert(rich4_game_event_insurance_cause(&event) == 5u);

    event = rich4_game_event_insurance_claim(1, 3, 1, 2000, 2000, 4u);
    assert(event.type == GAME_EVENT_INSURANCE_CLAIM_PRESENTATION_REQUESTED);
    assert(rich4_game_event_insurance_company(&event) == 3);
    assert(rich4_game_event_insurance_claimant(&event) == 1);
    assert(rich4_game_event_insurance_amount(&event) == 2000);

    event = rich4_game_event_special_event_presentation(2, 65, 66, 1u);
    assert(rich4_game_event_special_event_resource(&event) == 65);
    assert(rich4_game_event_special_event_panel(&event) == 66);
    assert(rich4_game_event_special_event_family(&event) == 1u);

    rich4_game_event_queue_reset(&queue);
    assert(rich4_game_event_emit_route_event_resolved(
        &queue, 1, 2, 3, 4, 5));
    assert(rich4_game_event_queue_pop(&queue, &event));
    assert(event.payload.kind == RICH4_GAME_EVENT_PAYLOAD_ROUTE);
    assert(rich4_game_event_subject(&event) == 2);

    {
        Rich4GameEventFields fields = {
            0, 0, 11, 4, 9, subject, target
        };
        event = rich4_game_event_company_stock_issue_requested_from_fields(
            fields);
    }
    assert(rich4_game_event_is_valid(&event));
    assert(rich4_game_event_value(&event) == 11);
    assert(rich4_game_event_target_ref(&event).id == 1);

    {
        Rich4GameEventFields fields;
        size_t i;
        fields.player = 2;
        fields.subject = 7;
        fields.value = 11;
        fields.value2 = 13;
        fields.flags = 17u;
        fields.subject_ref = rich4_game_domain_ref(GAME_DOMAIN_LAND, 4);
        fields.target_ref = rich4_game_domain_ref(GAME_DOMAIN_PLAYER, 5);
        for (i = 0; i < sizeof(typed_event_contracts) /
                         sizeof(typed_event_contracts[0]); i++) {
            Rich4GameEventFields actual;
            event = typed_event_contracts[i].construct(fields);
            assert(event.type == typed_event_contracts[i].type);
            assert(event.payload.kind ==
                   rich4_game_event_payload_kind(typed_event_contracts[i].type));
            assert(typed_event_contracts[i].validate(&event));
            actual = typed_event_contracts[i].access(&event);
            assert(actual.player == fields.player);
            assert(actual.subject == fields.subject);
            assert(actual.value == fields.value);
            assert(actual.value2 == fields.value2);
            assert(actual.flags == fields.flags);
            assert(actual.subject_ref.kind == fields.subject_ref.kind);
            assert(actual.target_ref.id == fields.target_ref.id);
        }
        event.payload.kind = RICH4_GAME_EVENT_PAYLOAD_NONE;
        assert(!rich4_game_event_is_valid(&event));
    }
    puts("GameEvent tagged payload boundary: ok");
    return 0;
}
