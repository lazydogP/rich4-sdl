#include "core/game_event_payload.h"

#include <stddef.h>

static GameDomainRef none_domain_ref(void)
{
    GameDomainRef value;
    value.kind = GAME_DOMAIN_NONE;
    value.id = -1;
    return value;
}

int rich4_game_event_beggar_player(const GameEvent *event)
{
    GameDomainRef target;
    if (event == NULL || event->type !=
            GAME_EVENT_BEGGAR_DONATION_PRESENTATION_REQUESTED)
        return -1;
    target = rich4_game_event_target_ref(event);
    return target.kind == GAME_DOMAIN_PLAYER ? target.id : -1;
}

int rich4_game_event_beggar_node(const GameEvent *event)
{
    return event != NULL && event->type ==
            GAME_EVENT_BEGGAR_DONATION_PRESENTATION_REQUESTED
        ? rich4_game_event_value2(event) : -1;
}

int rich4_game_event_beggar_amount(const GameEvent *event)
{
    return event != NULL && event->type ==
            GAME_EVENT_BEGGAR_DONATION_PRESENTATION_REQUESTED
        ? rich4_game_event_value(event) : 0;
}

unsigned int rich4_game_event_beggar_turn_token(const GameEvent *event)
{
    return event != NULL && event->type ==
            GAME_EVENT_BEGGAR_DONATION_PRESENTATION_REQUESTED
        ? rich4_game_event_flags(event) : 0u;
}

int rich4_game_event_special_event_resource(const GameEvent *event)
{
    return event != NULL &&
                   event->type == GAME_EVENT_SPECIAL_EVENT_PRESENTATION_REQUESTED
        ? rich4_game_event_subject(event) : 0;
}

int rich4_game_event_special_event_panel(const GameEvent *event)
{
    return event != NULL &&
                   event->type == GAME_EVENT_SPECIAL_EVENT_PRESENTATION_REQUESTED
        ? rich4_game_event_value(event) : 0;
}

unsigned int rich4_game_event_special_event_family(const GameEvent *event)
{
    return event != NULL &&
                   event->type == GAME_EVENT_SPECIAL_EVENT_PRESENTATION_REQUESTED
        ? rich4_game_event_flags(event) : 0u;
}

int rich4_game_event_facility_mode(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_FACILITY_ROULETTE_REQUESTED
        ? rich4_game_event_subject(event) : -1;
}

GameDomainRef rich4_game_event_facility_subject(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_FACILITY_ROULETTE_REQUESTED
        ? rich4_game_event_subject_ref(event) : none_domain_ref();
}

int rich4_game_event_facility_panel_resource(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_FACILITY_ROULETTE_REQUESTED
        ? rich4_game_event_value(event) : 0;
}

int rich4_game_event_facility_data_resource(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_FACILITY_ROULETTE_REQUESTED
        ? rich4_game_event_value2(event) : 0;
}

int rich4_game_event_company_issue_company(const GameEvent *event)
{
    return event != NULL &&
                   event->type == GAME_EVENT_COMPANY_STOCK_ISSUE_REQUESTED
        ? rich4_game_event_subject(event) : -1;
}

GameDomainRef rich4_game_event_company_issue_stock(const GameEvent *event)
{
    return event != NULL &&
                   event->type == GAME_EVENT_COMPANY_STOCK_ISSUE_REQUESTED
        ? rich4_game_event_target_ref(event) : none_domain_ref();
}

int rich4_game_event_company_issue_price(const GameEvent *event)
{
    return event != NULL &&
                   event->type == GAME_EVENT_COMPANY_STOCK_ISSUE_REQUESTED
        ? rich4_game_event_value(event) : 0;
}

int rich4_game_event_company_issue_maximum(const GameEvent *event)
{
    return event != NULL &&
                   event->type == GAME_EVENT_COMPANY_STOCK_ISSUE_REQUESTED
        ? rich4_game_event_value2(event) : 0;
}

int rich4_game_event_shop_action(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_SHOP_VISIT_REQUESTED
        ? rich4_game_event_subject(event) : -1;
}

GameDomainRef rich4_game_event_shop_gift(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_SHOP_VISIT_REQUESTED
        ? rich4_game_event_subject_ref(event) : none_domain_ref();
}

int rich4_game_event_magic_house_auto_action(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_MAGIC_HOUSE_REQUESTED
        ? rich4_game_event_value(event) - 1 : -1;
}

int rich4_game_event_magic_house_condition(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_MAGIC_HOUSE_REQUESTED
        ? rich4_game_event_subject(event) : -1;
}

unsigned int rich4_game_event_magic_house_target_mask(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_MAGIC_HOUSE_REQUESTED
        ? rich4_game_event_flags(event) : 0u;
}

int rich4_game_event_point_action(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_POINT_MINIGAME_REQUESTED
        ? rich4_game_event_subject(event) : -1;
}

int rich4_game_event_point_panel_resource(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_POINT_MINIGAME_REQUESTED
        ? rich4_game_event_value(event) : 0;
}

int rich4_game_event_point_data_resource(const GameEvent *event)
{
    return event != NULL && event->type == GAME_EVENT_POINT_MINIGAME_REQUESTED
        ? rich4_game_event_value2(event) : 0;
}
