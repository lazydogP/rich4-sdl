#ifndef RICH4_CORE_FACILITY_H
#define RICH4_CORE_FACILITY_H

#include "core/ledger.h"

typedef enum GameFacilityActionKind {
    GAME_FACILITY_ACTION_INVALID = 0,
    GAME_FACILITY_ACTION_NONE,
    GAME_FACILITY_ACTION_DEFERRED,
    GAME_FACILITY_ACTION_TRAFFIC_CHARGE,
    GAME_FACILITY_ACTION_PURCHASE,
    GAME_FACILITY_ACTION_BUILD,
    GAME_FACILITY_ACTION_UPGRADE,
    GAME_FACILITY_ACTION_RESEARCH
} GameFacilityActionKind;

typedef struct GameFacilityAction {
    GameFacilityActionKind kind;
    int facility;
    int actor;
    int owner;
    int amount;
} GameFacilityAction;

typedef enum GameLandingResultKind {
    GAME_LANDING_INVALID = 0,
    GAME_LANDING_NONE,
    GAME_LANDING_APPLIED,
    GAME_LANDING_CHARGED,
    GAME_LANDING_BANKRUPTCY
} GameLandingResultKind;

typedef struct GameFacilityLandingResult {
    GameLandingResultKind kind;
    int payer;
    int owner;
    int outcome;
    int amount;
} GameFacilityLandingResult;

typedef struct GameCompanyLandingResult {
    GameLandingResultKind kind;
    int company;
    int stock;
    int payer;
    int chairman;
    int outcome;
    int amount;
} GameCompanyLandingResult;

int rich4_facility_purchase_price(const GameState *state, int facility,
                                  int *out_price);
int rich4_facility_upgrade_price(const GameState *state, int facility,
                                 int *out_price);
int rich4_facility_asset_value(const GameState *state, int facility,
                               int *out_value);
/* Quote the type-3 traffic fee before card redirects/free-charge effects. */
int rich4_facility_traffic_charge_amount(
    const GameState *state, int actor, int facility, int dice_total,
    int *out_amount);
int rich4_facility_inspect(const GameState *state, int actor, int facility,
                           GameFacilityAction *out_action);
int rich4_facility_apply_command(GameState *state, const GameCommand *command,
                                 GameEventQueue *events);
GamePaymentResult rich4_facility_resolve_landing(
    GameState *state, int actor, int facility, int outcome, int dice_total,
    int accept_free, int redirect_target, GameFacilityLandingResult *out,
    GameEventQueue *events);
int rich4_facility_start_research(GameState *state, int actor, int facility,
                                  int selection, GameEventQueue *events);
int rich4_facility_tick_research(GameState *state, int actor,
                                 GameEventQueue *events);

int rich4_company_landing_amount(const GameState *state, int actor, int company,
                                 int outcome, int dice_total, int *out_amount);
GamePaymentResult rich4_company_resolve_landing(
    GameState *state, int actor, int company, int outcome, int dice_total,
    int accept_free, int redirect_target, GameCompanyLandingResult *out,
    GameEventQueue *events);
int rich4_company_prepare_insurance(GameState *state, int actor, int company,
                                    int outcome, GameEventQueue *events);
GamePaymentResult rich4_company_pay_insurance(
    GameState *state, int actor, int company, int outcome,
    GameCompanyLandingResult *out, GameEventQueue *events);
GamePaymentResult rich4_company_resolve_construction(
    GameState *state, int actor, int company, GameDomainRef target,
    int facility_type, int accept_free, int redirect_target,
    GameCompanyLandingResult *out, GameEventQueue *events);
GamePaymentResult rich4_company_prepare_construction(
    GameState *state, int actor, int company, GameDomainRef target,
    int facility_type, GameCompanyLandingResult *out, GameEventQueue *events);
GamePaymentResult rich4_company_pay_construction(
    GameState *state, int actor, int company, int quote,
    GameCompanyLandingResult *out, GameEventQueue *events);
int rich4_company_max_issue_purchase(const GameState *state, int actor,
                                     int company, int *out_price,
                                     int *out_maximum);
int rich4_company_buy_issued_stock(GameState *state, int actor, int company,
                                   int quantity, GameEventQueue *events);

#endif
