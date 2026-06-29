#ifndef RICH4_APP_ROUTE_FLOW_H
#define RICH4_APP_ROUTE_FLOW_H

#include "app/game_session.h"
#include "core/game_command.h"
#include "core/route_event.h"

typedef enum Rich4AppRouteInteractionChoice {
    RICH4_APP_ROUTE_INTERACTION_ACCEPT = 0,
    RICH4_APP_ROUTE_INTERACTION_DECLINE
} Rich4AppRouteInteractionChoice;

void rich4_app_route_reset_facts(GameSession *session);
void rich4_app_route_reset_dispatch(GameSession *session);
int rich4_app_route_advance_dispatch(GameSession *session);
int rich4_app_route_dispatch_current_handler(GameSession *session);
int rich4_app_route_defer_current_event(GameSession *session);
void rich4_app_route_prepare_dispatch(GameSession *session, int player_index);
void rich4_app_route_set_hover(GameSession *session, int hover);
void rich4_app_route_set_pressed(GameSession *session, int pressed);
int rich4_app_route_interaction_is_mandatory_facility_build(
    const GameSession *session);
int rich4_app_route_interaction_is_facility_research(
    const GameSession *session);
int rich4_app_route_interaction_is_facility_choice(
    const GameSession *session);
int rich4_app_route_interaction_uses_confirm_dialog(
    const GameSession *session);
int rich4_app_route_facility_choice_at(int x, int y);
int rich4_app_route_facility_research_choice_at(int x, int y);
GameCommand rich4_app_route_facility_build_command(
    const GameSession *session, int choice);
GameCommand rich4_app_route_facility_research_command(
    const GameSession *session, int choice);
int rich4_app_route_interaction_command_for_choice(
    const GameSession *session, Rich4AppRouteInteractionChoice choice,
    GameCommand *out_command);

#endif
