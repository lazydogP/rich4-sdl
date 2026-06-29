#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/ai.h"

static void setup(GameState *state, int personality)
{
    memset(state, 0, sizeof(*state));
    state->active = 1;
    state->player_count = 2;
    state->current_player = 0;
    state->price_index = 1;
    state->players[0].is_ai = 1;
    state->players[0].play_state = 2;
    state->players[0].ai_action_flags = 3;
    state->players[0].ai_personality = personality;
    state->players[0].attached_timed_bomb_slot = -1;
    state->players[1].play_state = 1;
    state->players[1].attached_timed_bomb_slot = -1;
}

static int has_result(const GameAiDecision *d, GameAiCandidateResult result)
{
    int i;
    for (i = 0; i < d->candidate_count; i++)
        if (d->candidates[i].result == result) return 1;
    return 0;
}

static uint32_t find_card_seed(const GameState *state, int want_command,
                               GameAiCandidateResult result)
{
    uint32_t seed;
    for (seed = 1; seed < 10000; seed++) {
        GameAiDecision d;
        assert(rich4_ai_select_command(state, 0, GAME_AI_PHASE_CARD_OR_TOOL,
                                       seed, &d));
        if ((d.has_command != 0) == want_command && has_result(&d, result))
            return seed;
    }
    assert(!"card seed not found");
    return 0;
}

static GameAiDecision select_context_card(const GameState *state,
                                          GameDomainRef current_property,
                                          const uint8_t *player_on_map,
                                          int card)
{
    GameAiDecision decision;
    uint32_t seed;
    for (seed = 1; seed < 10000; seed++) {
        assert(rich4_ai_select_command_with_map(
            state, current_property, player_on_map, 0,
            GAME_AI_PHASE_CARD_OR_TOOL,
            seed, &decision));
        if (decision.has_command &&
            decision.command.type == GAME_COMMAND_USE_CARD &&
            decision.command.subject.id == card)
            return decision;
    }
    assert(!"context card seed not found");
    memset(&decision, 0, sizeof(decision));
    return decision;
}

static GameAiDecision select_context_card_candidate(
    const GameState *state, GameDomainRef current_property,
    const uint8_t *player_on_map, int card)
{
    GameAiDecision decision;
    uint32_t seed;
    for (seed = 1; seed < 10000; seed++) {
        int i;
        assert(rich4_ai_select_command_with_map(
            state, current_property, player_on_map, 0,
            GAME_AI_PHASE_CARD_OR_TOOL,
            seed, &decision));
        for (i = 0; i < decision.candidate_count; i++)
            if (decision.candidates[i].subject.kind == GAME_DOMAIN_CARD &&
                decision.candidates[i].subject.id == card)
                return decision;
    }
    assert(!"context card candidate not found");
    memset(&decision, 0, sizeof(decision));
    return decision;
}

static void personalities_and_cards(void)
{
    GameState state, before;
    GameAiDecision a, b;
    uint32_t seed;
    assert(strcmp(rich4_ai_personality_name(GAME_AI_GOOD_CHILD), "乖宝宝") == 0);
    assert(strcmp(rich4_ai_personality_name(GAME_AI_ORDINARY), "普通人") == 0);
    assert(strcmp(rich4_ai_personality_name(GAME_AI_SCHEMER), "大老奸") == 0);

    setup(&state, GAME_AI_GOOD_CHILD);
    state.players[0].card_inventory[0] = 1; /* 均富卡, difficulty 2 */
    state.players[0].cash = 100;
    state.players[1].cash = 2000;
    state.players[0].tool_inventory[0] = 0;
    seed = find_card_seed(&state, 0, GAME_AI_CANDIDATE_DIFFICULTY);
    before = state;
    assert(rich4_ai_select_command(&state, 0, GAME_AI_PHASE_CARD_OR_TOOL,
                                   seed, &a));
    assert(memcmp(&state, &before, sizeof(state)) == 0);

    state.players[0].ai_personality = GAME_AI_ORDINARY;
    seed = find_card_seed(&state, 0, GAME_AI_CANDIDATE_RANDOM_GATE);
    assert(rich4_ai_select_command(&state, 0, GAME_AI_PHASE_CARD_OR_TOOL,
                                   seed, &a));
    seed = find_card_seed(&state, 1, GAME_AI_CANDIDATE_SELECTED);
    assert(rich4_ai_select_command(&state, 0, GAME_AI_PHASE_CARD_OR_TOOL,
                                   seed, &a));
    assert(a.command.type == GAME_COMMAND_USE_CARD && a.command.subject.id == 0);

    state.players[0].ai_personality = GAME_AI_SCHEMER;
    seed = find_card_seed(&state, 1, GAME_AI_CANDIDATE_SELECTED);
    assert(rich4_ai_select_command(&state, 0, GAME_AI_PHASE_CARD_OR_TOOL,
                                   seed, &a));
    assert(rich4_ai_select_command(&state, 0, GAME_AI_PHASE_CARD_OR_TOOL,
                                   seed, &b));
    assert(memcmp(&a, &b, sizeof(a)) == 0);
}

static void tools_and_deferred_position(void)
{
    GameState state;
    GameAiDecision d;
    uint32_t seed;
    setup(&state, GAME_AI_SCHEMER);
    state.players[0].ai_action_flags = 2;
    state.players[0].tool_inventory[1] = 1; /* route target remains evidence-deferred */
    for (seed = 1; seed < 10000; seed++) {
        assert(rich4_ai_select_command(&state, 0, GAME_AI_PHASE_CARD_OR_TOOL,
                                       seed, &d));
        if (has_result(&d, GAME_AI_CANDIDATE_DEFERRED_TARGET)) break;
    }
    assert(seed < 10000 && !d.has_command);
    memset(state.players[0].tool_inventory, 0,
           sizeof(state.players[0].tool_inventory));
    state.players[0].tool_inventory[4] = 1; /* motorcycle, no target */
    for (seed = 1; seed < 10000; seed++) {
        assert(rich4_ai_select_command(&state, 0, GAME_AI_PHASE_CARD_OR_TOOL,
                                       seed, &d));
        if (d.has_command) break;
    }
    assert(seed < 10000 && d.command.type == GAME_COMMAND_USE_TOOL &&
           d.command.subject.id == 4);

    memset(state.players[0].tool_inventory, 0,
           sizeof(state.players[0].tool_inventory));
    state.players[0].tool_inventory[7] = 1;
    state.players[0].cash = 10000;
    seed = find_card_seed(&state, 0, GAME_AI_CANDIDATE_DEFERRED_TARGET);
    assert(rich4_ai_select_command(&state, 0, GAME_AI_PHASE_CARD_OR_TOOL,
                                   seed, &d));
    assert(!d.has_command && d.candidate_count == 4);

}

static void projected_ai_context_fallback(void)
{
    GameState state;
    GameAiDecision decision;
    int16_t route_x[3] = {0, 0, 100};
    int16_t route_y[3] = {0, 0, 0};
    uint16_t route_next[3][4] = {{0}};
    int16_t land_x[1] = {0};
    int16_t land_y[1] = {0};
    uint32_t seed;

    /* Direct core callers no longer need to manufacture player_on_map when
     * route geometry marks the active players as visible. */
    setup(&state, GAME_AI_SCHEMER);
    state.players[0].ai_action_flags = 1;
    state.players[0].card_inventory[0] = 26; /* tax */
    state.players[0].node = 1;
    state.players[1].node = 2;
    state.players[1].cash = 60000;
    assert(rich4_tool_set_map_geometry(
        &state, 3, route_x, route_y, route_next, 0, NULL, NULL,
        0, NULL, NULL));
    for (seed = 1; seed < 10000; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        if (decision.has_command) break;
    }
    assert(seed < 10000 && decision.command.subject.id == 25 &&
           decision.command.target.kind == GAME_DOMAIN_PLAYER &&
           decision.command.target.id == 1);

    /* When route objects carry the original high-bit player mask, it is
     * authoritative over the typed route-node fallback.  Player 2 is active
     * and wealthy but absent from the visible mask, so the tax card must not
     * widen the target set to include it. */
    setup(&state, GAME_AI_SCHEMER);
    state.player_count = 3;
    state.players[0].ai_action_flags = 1;
    state.players[0].card_inventory[0] = 26; /* tax */
    state.players[0].node = 1;
    state.players[1].node = 2;
    state.players[1].cash = 100;
    state.players[2].node = 2;
    state.players[2].cash = 60000;
    state.route_object_raw[2] = 0x8002; /* only ordinary player 1 visible */
    assert(rich4_tool_set_map_geometry(
        &state, 3, route_x, route_y, route_next, 0, NULL, NULL,
        0, NULL, NULL));
    assert(rich4_ai_select_command(
        &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, 1, &decision));
    assert(!decision.has_command);

    /* An authoritative mask can contain only inactive/out-of-range bits.
     * It still suppresses the typed fallback; otherwise the wealthy active
     * player 2 would become an invalid tax target. */
    setup(&state, GAME_AI_SCHEMER);
    state.player_count = 3;
    state.players[0].ai_action_flags = 1;
    state.players[0].card_inventory[0] = 26; /* tax */
    state.players[0].node = 1;
    state.players[1].node = 2;
    state.players[1].cash = 100;
    state.players[2].node = 2;
    state.players[2].cash = 60000;
    state.route_object_raw[2] = 0x8008; /* only an unavailable bit */
    assert(rich4_tool_set_map_geometry(
        &state, 3, route_x, route_y, route_next, 0, NULL, NULL,
        0, NULL, NULL));
    assert(rich4_ai_select_command(
        &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, 1, &decision));
    assert(!decision.has_command);

    /* A raw property object at the actor's route node supplies the current
     * property when the app has not passed a stop-event reference. */
    setup(&state, GAME_AI_SCHEMER);
    state.players[0].ai_action_flags = 1;
    state.players[0].card_inventory[0] = 8; /* auction */
    state.players[0].node = 1;
    state.players[1].node = 2;
    state.land_count = 1;
    state.land_owner[0] = 1;
    state.land_level[0] = 3;
    state.route_object_raw[1] = 0x7d1;
    assert(rich4_tool_set_map_geometry(
        &state, 3, route_x, route_y, route_next, 1, land_x, land_y,
        0, NULL, NULL));
    for (seed = 1; seed < 10000; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        if (decision.has_command) break;
    }
    assert(seed < 10000 && decision.command.subject.id == 7 &&
           decision.command.target.kind == GAME_DOMAIN_LAND &&
           decision.command.target.id == 0);
}

static void remote_dice_ai_gates(void)
{
    GameState state;
    GameAiDecision decision;
    int16_t route_x[4] = {0, 0, 100, 200};
    int16_t route_y[4] = {0, 0, 0, 0};
    int16_t land_x[3] = {100, 200, 300};
    int16_t land_y[3] = {0, 0, 0};
    uint16_t route_next[4][4] = {{0}};
    uint32_t seed;
    int i;
    int saw_rejected;
    int saw_no_budget;

    /* 0x421827 rejects the three god slots before its target scan. */
    setup(&state, GAME_AI_SCHEMER);
    state.players[0].ai_action_flags = 2;
    state.players[0].tool_inventory[7] = 1;
    state.players[0].cash = 10000;
    state.players[0].god_info = 7;
    saw_rejected = 0;
    for (seed = 1; seed < 10000 && !saw_rejected; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        for (i = 0; i < decision.candidate_count; i++) {
            if (decision.candidates[i].subject.kind == GAME_DOMAIN_TOOL &&
                decision.candidates[i].subject.id == 7 &&
                decision.candidates[i].result == GAME_AI_CANDIDATE_RULE_REJECTED) {
                saw_rejected = 1;
                break;
            }
        }
    }
    assert(saw_rejected && !decision.has_command);

    setup(&state, GAME_AI_SCHEMER);
    state.players[0].ai_action_flags = 2;
    state.players[0].tool_inventory[7] = 1;
    state.players[0].cash = 10000;
    state.players[0].days_tortoise_walking = 1;
    saw_rejected = 0;
    for (seed = 1; seed < 10000 && !saw_rejected; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        for (i = 0; i < decision.candidate_count; i++) {
            if (decision.candidates[i].subject.kind == GAME_DOMAIN_TOOL &&
                decision.candidates[i].subject.id == 7 &&
                decision.candidates[i].result == GAME_AI_CANDIDATE_RULE_REJECTED) {
                saw_rejected = 1;
                break;
            }
        }
    }
    assert(saw_rejected && !decision.has_command);

    setup(&state, GAME_AI_SCHEMER);
    state.players[0].ai_action_flags = 2;
    state.players[0].tool_inventory[7] = 1;
    state.players[0].cash = 9999;
    saw_no_budget = 0;
    for (seed = 1; seed < 10000 && !saw_no_budget; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        for (i = 0; i < decision.candidate_count; i++) {
            if (decision.candidates[i].subject.kind == GAME_DOMAIN_TOOL &&
                decision.candidates[i].subject.id == 7 &&
                decision.candidates[i].result == GAME_AI_CANDIDATE_NO_BUDGET) {
                saw_no_budget = 1;
                break;
            }
        }
    }
    assert(saw_no_budget && !decision.has_command);

    setup(&state, GAME_AI_SCHEMER);
    state.players[0].ai_action_flags = 2;
    state.players[0].tool_inventory[7] = 1;
    state.players[0].cash = 10000;
    state.players[0].god_investment_modifier = -1;
    saw_rejected = 0;
    for (seed = 1; seed < 10000 && !saw_rejected; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        for (i = 0; i < decision.candidate_count; i++) {
            if (decision.candidates[i].subject.kind == GAME_DOMAIN_TOOL &&
                decision.candidates[i].subject.id == 7 &&
                decision.candidates[i].result == GAME_AI_CANDIDATE_RULE_REJECTED) {
                saw_rejected = 1;
                break;
            }
        }
    }
    assert(saw_rejected && !decision.has_command);

    /* 0x421827 accepts affordable unowned land when two same-name
     * records already belong to the actor; the result is the route step. */
    setup(&state, GAME_AI_SCHEMER);
    state.players[0].ai_action_flags = 2;
    state.players[0].tool_inventory[7] = 1;
    state.players[0].cash = 10000;
    state.players[0].node = 1;
    route_next[1][0] = 2;
    route_next[2][0] = 3;
    assert(rich4_tool_set_map_geometry(
        &state, 4, route_x, route_y, route_next, 3, land_x, land_y,
        0, NULL, NULL));
    state.land_count = 3;
    state.land_group[0] = 4;
    state.land_group[1] = 4;
    state.land_group[2] = 4;
    state.land_owner[0] = -1;
    state.land_owner[1] = 0;
    state.land_owner[2] = 0;
    state.land_level[0] = 1;
    state.land_level[1] = 1;
    state.land_level[2] = 1;
    state.land_price[0] = 100;
    state.land_price_per_level[0] = 100;
    state.land_price_status[0] = 0;
    state.route_object_raw[2] = 0x7d1;
    state.price_index = 100; /* Remote dice reads the raw price. */
    state.players[0].delayed_status_block = 1;
    for (seed = 1; seed < 10000; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        if (decision.has_command) break;
    }
    assert(seed < 10000 && decision.command.value == 1);
    state.land_owner[0] = 0;
    for (seed = 1; seed < 10000; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        if (decision.has_command) break;
    }
    assert(seed < 10000 && decision.has_command &&
           decision.command.subject.id == 7 &&
           decision.command.target.kind == GAME_DOMAIN_NONE &&
           decision.command.value >= 1 && decision.command.value <= 6);

    /* The parity overlay is not part of 0x421827's typed price gate. */
    state.land_price_status[0] = 1;
    for (seed = 1; seed < 10000; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        if (decision.has_command) break;
    }
    assert(seed < 10000 && decision.has_command &&
           decision.command.subject.id == 7);

    /* The strict cash comparison is 2.5 times raw construction price. */
    state.land_price_per_level[0] = 5000;
    saw_rejected = 0;
    for (seed = 1; seed < 10000 && !saw_rejected; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        for (i = 0; i < decision.candidate_count; i++) {
            if (decision.candidates[i].subject.kind == GAME_DOMAIN_TOOL &&
                decision.candidates[i].subject.id == 7 &&
                decision.candidates[i].result ==
                    GAME_AI_CANDIDATE_RULE_REJECTED) {
                saw_rejected = 1;
                break;
            }
        }
    }
    assert(saw_rejected && !decision.has_command);

    /* A branch in the six-node route window is also a known rejection; the
     * original fcn_0040B221 returns its branch marker to 0x421827. */
    state.land_price[0] = 100;
    state.land_price_status[0] = 0;
    route_next[1][1] = 3;
    assert(rich4_tool_set_map_geometry(
        &state, 4, route_x, route_y, route_next, 3, land_x, land_y,
        0, NULL, NULL));
    saw_rejected = 0;
    for (seed = 1; seed < 10000 && !saw_rejected; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        for (i = 0; i < decision.candidate_count; i++) {
            if (decision.candidates[i].subject.kind == GAME_DOMAIN_TOOL &&
                decision.candidates[i].subject.id == 7 &&
                decision.candidates[i].result ==
                    GAME_AI_CANDIDATE_RULE_REJECTED) {
                saw_rejected = 1;
                break;
            }
        }
    }
    assert(saw_rejected && !decision.has_command);

    /* 0x421827 reads the fixed +0x24 facility charge (charge level 0), even
     * when the owned facility is level 1. The parity overlay is not a gate. */
    setup(&state, GAME_AI_SCHEMER);
    state.players[0].ai_action_flags = 2;
    state.players[0].tool_inventory[7] = 1;
    state.players[0].cash = 10000;
    state.players[0].node = 1;
    route_next[1][0] = 2;
    route_next[1][1] = 0;
    route_next[2][0] = 0;
    assert(rich4_tool_set_map_geometry(
        &state, 3, route_x, route_y, route_next, 0, NULL, NULL,
        0, NULL, NULL));
    state.facility_count = 1;
    state.facility_owner[0] = 0;
    state.facility_type[0] = 1;
    state.facility_level[0] = 1;
    state.facility_price_status[0] = 1;
    state.facility_charge_by_level[0][0] = 0;
    state.facility_charge_by_level[0][1] = 4000;
    state.route_object_raw[2] = 0xfa1;
    for (seed = 1; seed < 10000; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        if (decision.has_command) break;
    }
    assert(seed < 10000 && decision.has_command &&
           decision.command.subject.id == 7);
    /* A middle route selector is not automatically a rejection: the
     * original object-category table only skips its special categories. */
    state.tool_route_flags[2] = 1u << 16; /* original category 1 */
    for (seed = 1; seed < 10000; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        if (decision.has_command) break;
    }
    assert(seed < 10000 && decision.has_command &&
           decision.command.subject.id == 7);
    state.tool_route_flags[2] = 5u << 16; /* original skipped category 5 */
    saw_rejected = 0;
    for (seed = 1; seed < 10000 && !saw_rejected; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        for (i = 0; i < decision.candidate_count; i++) {
            if (decision.candidates[i].subject.kind == GAME_DOMAIN_TOOL &&
                decision.candidates[i].subject.id == 7 &&
                decision.candidates[i].result ==
                    GAME_AI_CANDIDATE_RULE_REJECTED) {
                saw_rejected = 1;
                break;
            }
        }
    }
    assert(saw_rejected && !decision.has_command);
    state.tool_route_flags[2] = 0;
    state.facility_charge_by_level[0][0] = 4000;
    state.facility_charge_by_level[0][1] = 1000;
    saw_rejected = 0;
    for (seed = 1; seed < 10000 && !saw_rejected; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        for (i = 0; i < decision.candidate_count; i++) {
            if (decision.candidates[i].subject.kind == GAME_DOMAIN_TOOL &&
                decision.candidates[i].subject.id == 7 &&
                decision.candidates[i].result ==
                    GAME_AI_CANDIDATE_RULE_REJECTED) {
                saw_rejected = 1;
                break;
            }
        }
    }
    assert(saw_rejected && !decision.has_command);

}

static void route_tool_target_window(void)
{
    GameState state;
    GameAiDecision decision;
    uint8_t placeable[7] = {0, 1, 1, 1, 1, 1, 1};
    int16_t route_x[7] = {0, 0, 10, 20, 30, 40, 50};
    int16_t route_y[7] = {0, 0, 0, 0, 0, 0, 0};
    uint16_t route_next[7][4] = {{0}};
    uint32_t seed;

    setup(&state, GAME_AI_SCHEMER);
    state.players[0].ai_action_flags = 2;
    state.players[0].tool_inventory[1] = 1; /* roadblock */
    state.players[0].node = 1;
    route_next[1][0] = 2;
    route_next[1][1] = 3;
    route_next[2][0] = 4;
    route_next[3][0] = 5;
    route_next[5][0] = 6;
    assert(rich4_tool_set_route_targets(
        &state, 7, placeable, sizeof(placeable)));
    assert(rich4_tool_set_map_geometry(
        &state, 7, route_x, route_y, route_next, 0, NULL, NULL,
        0, NULL, NULL));
    /* The first branch is blocked, so the common 0x40B221 walk must use 3. */
    state.tool_route_edge_blocked[1] = 1u;
    state.players[0].points = 201;
    state.tool_route_flags[3] = 15;
    state.tool_route_flags[5] = 15;
    for (seed = 1; seed < 10000; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        if (decision.has_command) break;
    }
    assert(seed < 10000 && decision.command.type == GAME_COMMAND_USE_TOOL &&
           decision.command.subject.id == 1 &&
           decision.command.target.kind == GAME_DOMAIN_ROUTE_NODE &&
           decision.command.target.id == 3);

    /* Live objects are excluded while selecting, before invoking a handler. */
    state.tool_placed_objects[0].kind = 0x10;
    state.tool_placed_objects[0].node = 3;
    state.tool_placed_objects[0].owner = 1;
    assert(rich4_ai_select_command(
        &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, 1, &decision));
    assert(decision.has_command && decision.command.target.id == 5);
    state.tool_placed_objects[0].kind = 0;
    {
        GameAiGlobalProjection projection;
        uint16_t occupied_node = 3;
        memset(&projection, 0, sizeof(projection));
        projection.occupied_dynamic_nodes = &occupied_node;
        projection.occupied_dynamic_node_count = 1;
        assert(rich4_ai_select_command_with_map_context_and_projection(
            &state, rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), NULL,
            NULL, &projection, 0, GAME_AI_PHASE_CARD_OR_TOOL, 1, &decision));
        assert(decision.has_command && decision.command.target.id == 5);
    }
    state.players[1].node = 3;
    assert(rich4_ai_select_command(
        &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, 1, &decision));
    assert(decision.has_command && decision.command.target.id == 5);
    state.players[1].days_in_hospital = 1;
    assert(rich4_ai_select_command(
        &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, 1, &decision));
    assert(decision.has_command && decision.command.target.id == 3);
    state.players[1].days_in_hospital = 0;
    state.players[1].node = 0;
    state.special_player_node[0] = 3;
    state.special_player_f10[0] = 0;
    assert(rich4_ai_select_command(
        &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, 1, &decision));
    assert(decision.has_command && decision.command.target.id == 5);

}

static void robot_tool_target_policy(void)
{
    GameState state;
    GameAiDecision decision;
    uint32_t seed;

    setup(&state, GAME_AI_SCHEMER);
    state.players[0].ai_action_flags = 2;
    state.players[0].cash = 0;
    state.players[0].tool_inventory[8] = 1; /* machine worker */
    state.land_count = 2;
    state.land_owner[0] = 0;
    state.land_level[0] = 1;
    state.land_toll_by_level[0][1] = 30;
    state.land_owner[1] = 0;
    state.land_level[1] = 2;
    state.land_toll_by_level[1][2] = 10;
    for (seed = 1; seed < 10000; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        if (decision.has_command) break;
    }
    assert(seed < 10000 && decision.command.type == GAME_COMMAND_USE_TOOL &&
           decision.command.subject.id == 8 &&
           decision.command.target.kind == GAME_DOMAIN_LAND &&
           decision.command.target.id == 0 && decision.command.value == 0);

    /* The original 0x421BA6 selector calls 0x40A45C(-1), so a higher-scoring
     * owned property outside the centered 29x29 table still wins. */
    {
        int16_t route_x[2] = {0, 320};
        int16_t route_y[2] = {0, 320};
        uint16_t route_next[2][4] = {{0}};
        int16_t land_x[2] = {320, 1000};
        int16_t land_y[2] = {320, 1000};

        setup(&state, GAME_AI_SCHEMER);
        state.players[0].ai_action_flags = 2;
        state.players[0].cash = 0;
        state.players[0].node = 1;
        state.players[0].tool_inventory[8] = 1;
        state.land_count = 2;
        state.land_owner[0] = state.land_owner[1] = 0;
        state.land_level[0] = 1;
        state.land_level[1] = 2;
        state.land_toll_by_level[0][1] = 30;
        state.land_toll_by_level[1][2] = 100;
        state.land_price_status[1] = 1;
        assert(rich4_tool_set_map_geometry(
            &state, 2, route_x, route_y, route_next, 2, land_x, land_y,
            0, NULL, NULL));
        for (seed = 1; seed < 10000; seed++) {
            assert(rich4_ai_select_command(
                &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
            if (decision.has_command) break;
        }
        assert(seed < 10000 && decision.command.subject.id == 8 &&
               decision.command.target.kind == GAME_DOMAIN_LAND &&
               decision.command.target.id == 1);
    }

    /* An untyped facility uses the original AI's 1..4 subtype choice and is
     * still checked by the shared property-target inspector. */
    setup(&state, GAME_AI_SCHEMER);
    state.players[0].ai_action_flags = 2;
    state.players[0].cash = 0;
    state.players[0].tool_inventory[8] = 1;
    state.facility_count = 1;
    state.facility_owner[0] = 0;
    state.facility_level[0] = 0;
    state.facility_charge_by_level[0][0] = 100;
    for (seed = 1; seed < 10000; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        if (decision.has_command) break;
    }
    assert(seed < 10000 && decision.command.subject.id == 8 &&
           decision.command.target.kind == GAME_DOMAIN_FACILITY &&
           decision.command.target.id == 0 && decision.command.value >= 1 &&
           decision.command.value <= 4);
}

static void area_and_transfer_tool_targets(void)
{
    GameState state;
    GameAiDecision decision;
    uint8_t placeable[4] = {0, 1, 1, 1};
    int16_t route_x[4] = {0, 0, 100, 150};
    int16_t route_y[4] = {0, 0, 0, 0};
    uint16_t route_next[4][4] = {{0}};
    uint8_t visible_placeable[7] = {0, 1, 1, 1, 1, 1, 1};
    int16_t visible_route_x[7] = {0, 0, 32, 64, 96, 128, 256};
    int16_t visible_route_y[7] = {0, 0, 0, 0, 0, 0, 0};
    uint16_t visible_route_next[7][4] = {{0}};
    int16_t visible_land_x[1] = {256};
    int16_t visible_land_y[1] = {0};
    uint32_t seed;
    int i;
    int saw_rejected;

    route_next[1][0] = 2;
    route_next[2][0] = 3;
    visible_route_next[1][0] = 2;
    visible_route_next[2][0] = 3;
    visible_route_next[3][0] = 4;
    visible_route_next[4][0] = 5;
    visible_route_next[5][0] = 6;

    setup(&state, GAME_AI_SCHEMER);
    state.players[0].ai_action_flags = 2;
    state.players[0].tool_inventory[6] = 1; /* missile */
    state.players[0].node = 1;
    state.players[1].node = 2;
    state.players[0].relation_balance[1] = 5;
    assert(rich4_tool_set_route_targets(
        &state, 4, placeable, sizeof(placeable)));
    assert(rich4_tool_set_map_geometry(
        &state, 4, route_x, route_y, route_next, 0, NULL, NULL,
        0, NULL, NULL));
    state.land_count = 2;
    state.land_owner[1] = 1;
    state.route_object_raw[3] = 0x7d2; /* other player's visible land */
    for (seed = 1; seed < 10000; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        if (decision.has_command) break;
    }
    assert(seed < 10000 && decision.command.type == GAME_COMMAND_USE_TOOL &&
           decision.command.subject.id == 6 &&
           decision.command.target.kind == GAME_DOMAIN_PLAYER &&
           decision.command.target.id == 1 &&
           rich4_tool_inspect(&state, &decision.command) ==
               GAME_TOOL_USE_APPLIED);

    /* Once the global object table contains the selected player's ordinary
     * mask, 0x421717 still checks the local 200x200 screen.  A high-bit actor
     * there rejects the target even when a foreign property is nearby. */
    state.route_object_raw[1] = 0x8002;
    state.tool_route_x[3] = 250;
    saw_rejected = 0;
    for (seed = 1; seed < 10000 && !saw_rejected; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        for (i = 0; i < decision.candidate_count; i++) {
            if (decision.candidates[i].subject.kind == GAME_DOMAIN_TOOL &&
                decision.candidates[i].subject.id == 6 &&
                decision.candidates[i].result ==
                    GAME_AI_CANDIDATE_RULE_REJECTED) {
                saw_rejected = 1;
                break;
            }
        }
    }
    assert(saw_rejected && !decision.has_command);

    /* If the global mask exists but omits the relation target, 0x421717
     * rejects before using a local fallback; it must not widen the target
     * search to another player. */
    state.route_object_raw[1] = 0x8001; /* only the acting player is visible */
    state.tool_route_x[3] = 250;
    saw_rejected = 0;
    for (seed = 1; seed < 10000 && !saw_rejected; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        for (i = 0; i < decision.candidate_count; i++) {
            if (decision.candidates[i].subject.kind == GAME_DOMAIN_TOOL &&
                decision.candidates[i].subject.id == 6 &&
                decision.candidates[i].result == GAME_AI_CANDIDATE_RULE_REJECTED) {
                saw_rejected = 1;
                break;
            }
        }
    }
    assert(saw_rejected && !decision.has_command);
    state.route_object_raw[1] = 0;
    state.tool_route_x[3] = 150;

    /* Once the only object in the 200x200 window is removed, the original
     * missile handler has no useful area target and the AI rejects it. */
    state.route_object_raw[3] = 0;
    saw_rejected = 0;
    for (seed = 1; seed < 10000 && !saw_rejected; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        for (i = 0; i < decision.candidate_count; i++) {
            if (decision.candidates[i].subject.kind == GAME_DOMAIN_TOOL &&
                decision.candidates[i].subject.id == 6 &&
                decision.candidates[i].result == GAME_AI_CANDIDATE_RULE_REJECTED) {
                saw_rejected = 1;
                break;
            }
        }
    }
    assert(saw_rejected && !decision.has_command);

    /* With no relation-preferred player, 0x40D31C chooses one eligible
     * player with the AI RNG.  It must not permanently stop at the first
     * active player when that player's 200x200 window is empty: a later
     * randomly selected player can still be the original valid center. */
    setup(&state, GAME_AI_SCHEMER);
    state.player_count = 3;
    state.players[0].ai_action_flags = 2;
    state.players[0].tool_inventory[6] = 1;
    state.players[0].node = 1;
    state.players[1].node = 2;
    state.players[2].node = 3;
    state.players[2].play_state = 1;
    assert(rich4_tool_set_route_targets(
        &state, 4, placeable, sizeof(placeable)));
    assert(rich4_tool_set_map_geometry(
        &state, 4, route_x, route_y, route_next, 0, NULL, NULL,
        0, NULL, NULL));
    state.land_count = 1;
    state.land_owner[0] = 2;
    state.route_object_raw[3] = 0x7d1;
    for (seed = 1; seed < 10000; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        if (decision.has_command && decision.command.subject.id == 6 &&
            decision.command.target.kind == GAME_DOMAIN_PLAYER &&
            decision.command.target.id == 2)
            break;
    }
    assert(seed < 10000);

    setup(&state, GAME_AI_SCHEMER);
    state.players[0].ai_action_flags = 2;
    state.players[0].tool_inventory[10] = 1; /* transfer machine */
    state.players[0].cash = 10001;
    state.players[0].node = 1;
    state.players[1].node = 2;
    assert(rich4_tool_set_route_targets(
        &state, 4, placeable, sizeof(placeable)));
    assert(rich4_tool_set_map_geometry(
        &state, 4, route_x, route_y, route_next, 0, NULL, NULL,
        0, NULL, NULL));
    seed = find_card_seed(&state, 0, GAME_AI_CANDIDATE_RULE_REJECTED);
    assert(rich4_ai_select_command(
        &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
    assert(!decision.has_command);

    /* With a projected forward land object, the original transfer gate also
     * requires an unowned, ordinary, level-3 property whose indexed price
     * fits the actor's cash.  The emitted command remains player -> route
     * node. */
    setup(&state, GAME_AI_SCHEMER);
    state.players[0].ai_action_flags = 2;
    state.players[0].tool_inventory[10] = 1;
    state.players[0].cash = 10001;
    state.players[0].node = 1;
    assert(rich4_tool_set_route_targets(
        &state, 4, placeable, sizeof(placeable)));
    assert(rich4_tool_set_map_geometry(
        &state, 4, route_x, route_y, route_next, 0, NULL, NULL,
        0, NULL, NULL));
    state.land_count = 1;
    state.land_owner[0] = -1;
    state.land_level[0] = 3;
    state.land_price_per_level[0] = 100;
    state.route_object_raw[2] = 0x7d1;
    for (seed = 1; seed < 10000; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        if (decision.has_command) break;
    }
    assert(seed < 10000 && decision.command.subject.id == 10 &&
           decision.command.target.kind == GAME_DOMAIN_PLAYER &&
           decision.command.target2.kind == GAME_DOMAIN_ROUTE_NODE &&
           decision.command.target2.id == 2);

    /* The original 29x29 object table can contain a typed property even when
     * its raw route record is absent.  The transfer command must still map
     * the selected property's coordinates back to the route node. */
    state.route_object_raw[2] = 0;
    state.tool_land_x[0] = 100;
    state.tool_land_y[0] = 0;
    for (seed = 1; seed < 10000; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        if (decision.has_command) break;
    }
    assert(seed < 10000 && decision.command.subject.id == 10 &&
           decision.command.target.kind == GAME_DOMAIN_PLAYER &&
           decision.command.target.id == 0 &&
           decision.command.target2.kind == GAME_DOMAIN_ROUTE_NODE &&
           decision.command.target2.id == 2);

    /* 0x421CB6 scans the current 29x29 object table, not just the first four
     * forward nodes.  A visible level-3 land at node 6 must therefore win
     * over the old PLAYER -> first-forward-node fallback. */
    setup(&state, GAME_AI_SCHEMER);
    state.players[0].ai_action_flags = 2;
    state.players[0].tool_inventory[10] = 1;
    state.players[0].cash = 10001;
    state.players[0].node = 1;
    assert(rich4_tool_set_route_targets(
        &state, 7, visible_placeable, sizeof(visible_placeable)));
    assert(rich4_tool_set_map_geometry(
        &state, 7, visible_route_x, visible_route_y, visible_route_next, 1,
        visible_land_x, visible_land_y, 0, NULL, NULL));
    state.land_count = 1;
    state.land_owner[0] = -1;
    state.land_type[0] = 0;
    state.land_level[0] = 3;
    state.land_price_per_level[0] = 100;
    state.route_object_raw[6] = 0x7d1;
    for (seed = 1; seed < 10000; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        if (decision.has_command) break;
    }
    assert(seed < 10000 && decision.command.subject.id == 10 &&
           decision.command.target.kind == GAME_DOMAIN_PLAYER &&
           decision.command.target.id == 0 &&
           decision.command.target2.kind == GAME_DOMAIN_ROUTE_NODE &&
           decision.command.target2.id == 6);

    /* The original keeps the highest qualifying level from the scan, rather
     * than the first qualifying route object.  The parity overlay is not the
     * transfer gate's type byte and must not disqualify an ordinary land. */
    setup(&state, GAME_AI_SCHEMER);
    state.players[0].ai_action_flags = 2;
    state.players[0].tool_inventory[10] = 1;
    state.players[0].cash = 10001;
    state.players[0].node = 1;
    assert(rich4_tool_set_route_targets(
        &state, 4, placeable, sizeof(placeable)));
    assert(rich4_tool_set_map_geometry(
        &state, 4, route_x, route_y, route_next, 0, NULL, NULL,
        0, NULL, NULL));
    state.land_count = 2;
    state.land_owner[0] = -1;
    state.land_owner[1] = -1;
    state.land_type[0] = 0;
    state.land_type[1] = 0;
    state.land_price_status[0] = 1;
    state.land_level[0] = 3;
    state.land_level[1] = 4;
    state.land_price_per_level[0] = 100;
    state.land_price_per_level[1] = 100;
    state.route_object_raw[2] = 0x7d1;
    state.route_object_raw[3] = 0x7d2;
    for (seed = 1; seed < 10000; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        if (decision.has_command) break;
    }
    assert(seed < 10000 && decision.command.subject.id == 10 &&
           decision.command.target2.kind == GAME_DOMAIN_ROUTE_NODE &&
           decision.command.target2.id == 3);

    /* The same typed path accepts a facility candidate when its original
     * nonzero-type/level/price fields pass the gate. */
    setup(&state, GAME_AI_SCHEMER);
    state.players[0].ai_action_flags = 2;
    state.players[0].tool_inventory[10] = 1;
    state.players[0].cash = 10001;
    state.players[0].node = 1;
    assert(rich4_tool_set_route_targets(
        &state, 4, placeable, sizeof(placeable)));
    assert(rich4_tool_set_map_geometry(
        &state, 4, route_x, route_y, route_next, 0, NULL, NULL,
        0, NULL, NULL));
    state.facility_count = 1;
    state.facility_owner[0] = -1;
    state.facility_type[0] = 1;
    state.facility_price_status[0] = 1;
    state.facility_level[0] = 3;
    state.facility_price_per_level[0] = 20000; /* not read by 0x421CB6 */
    state.facility_charge_by_level[0][0] = 100;
    state.route_object_raw[2] = 0xfa1;
    for (seed = 1; seed < 10000; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        if (decision.has_command) break;
    }
    assert(seed < 10000 && decision.command.subject.id == 10 &&
           decision.command.target2.kind == GAME_DOMAIN_ROUTE_NODE &&
           decision.command.target2.id == 2);

    /* A known property projection no longer falls through to an arbitrary
     * route destination when its type or indexed price fails. */
    setup(&state, GAME_AI_SCHEMER);
    state.players[0].ai_action_flags = 2;
    state.players[0].tool_inventory[10] = 1;
    state.players[0].cash = 10001;
    state.players[0].node = 1;
    assert(rich4_tool_set_route_targets(
        &state, 4, placeable, sizeof(placeable)));
    assert(rich4_tool_set_map_geometry(
        &state, 4, route_x, route_y, route_next, 0, NULL, NULL,
        0, NULL, NULL));
    state.land_count = 1;
    state.land_owner[0] = -1;
    state.land_level[0] = 3;
    state.land_type[0] = 1;
    state.land_price_per_level[0] = 20000;
    state.route_object_raw[2] = 0x7d1;
    saw_rejected = 0;
    for (seed = 1; seed < 10000 && !saw_rejected; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        for (i = 0; i < decision.candidate_count; i++) {
            if (decision.candidates[i].subject.kind == GAME_DOMAIN_TOOL &&
                decision.candidates[i].subject.id == 10 &&
                decision.candidates[i].result ==
                    GAME_AI_CANDIDATE_RULE_REJECTED) {
                saw_rejected = 1;
                break;
            }
        }
    }
    assert(saw_rejected && !decision.has_command);

    /* The shared +70 modifier gate is also checked before destination
     * selection, matching fcn_00421CB6. */
    setup(&state, GAME_AI_SCHEMER);
    state.players[0].ai_action_flags = 2;
    state.players[0].tool_inventory[10] = 1;
    state.players[0].cash = 10001;
    state.players[0].god_investment_modifier = -1;
    state.players[0].node = 1;
    assert(rich4_tool_set_route_targets(
        &state, 4, placeable, sizeof(placeable)));
    assert(rich4_tool_set_map_geometry(
        &state, 4, route_x, route_y, route_next, 0, NULL, NULL,
        0, NULL, NULL));
    saw_rejected = 0;
    for (seed = 1; seed < 10000 && !saw_rejected; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        for (i = 0; i < decision.candidate_count; i++) {
            if (decision.candidates[i].subject.kind == GAME_DOMAIN_TOOL &&
                decision.candidates[i].subject.id == 10 &&
                decision.candidates[i].result ==
                    GAME_AI_CANDIDATE_RULE_REJECTED) {
                saw_rejected = 1;
                break;
            }
        }
    }
    assert(saw_rejected && !decision.has_command);

    /* The original eligibility functions reject these cases before target
     * selection: traffic restrictions for missiles and the strict transfer
     * liquidity floor. */
    state.players[0].traffic_method = 2;
    state.players[0].tool_inventory[10] = 0;
    state.players[0].tool_inventory[6] = 1;
    state.players[0].cash = 10001;
    saw_rejected = 0;
    for (seed = 1; seed < 10000 && !saw_rejected; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        for (i = 0; i < decision.candidate_count; i++) {
            if (decision.candidates[i].subject.kind == GAME_DOMAIN_TOOL &&
                decision.candidates[i].subject.id == 6 &&
                decision.candidates[i].result ==
                    GAME_AI_CANDIDATE_RULE_REJECTED) {
                saw_rejected = 1;
                break;
            }
        }
    }
    assert(saw_rejected && !decision.has_command);

    setup(&state, GAME_AI_SCHEMER);
    state.players[0].ai_action_flags = 2;
    state.players[0].tool_inventory[10] = 1;
    state.players[0].cash = 10000;
    state.players[0].node = 1;
    assert(rich4_tool_set_route_targets(
        &state, 4, placeable, sizeof(placeable)));
    assert(rich4_tool_set_map_geometry(
        &state, 4, route_x, route_y, route_next, 0, NULL, NULL,
        0, NULL, NULL));
    saw_rejected = 0;
    for (seed = 1; seed < 10000 && !saw_rejected; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        for (i = 0; i < decision.candidate_count; i++) {
            if (decision.candidates[i].subject.kind == GAME_DOMAIN_TOOL &&
                decision.candidates[i].subject.id == 10 &&
                decision.candidates[i].result ==
                    GAME_AI_CANDIDATE_NO_BUDGET) {
                saw_rejected = 1;
                break;
            }
        }
    }
    assert(saw_rejected && !decision.has_command);
}

static void projected_global_object_subset(void)
{
    GameState state;
    GameAiDecision decision;
    uint8_t placeable[3] = {0, 1, 1};
    int16_t route_x[3] = {0, 0, 0};
    int16_t route_y[3] = {0, 0, 0};
    uint16_t route_next[3][4] = {{0}};
    int16_t land_x[1] = {32};
    int16_t land_y[1] = {0};
    uint32_t seed;
    int saw_rejected;
    int i;

    /* A typed map property is part of the original centered object table
     * even when no route node repeats its raw object ID.  The area missile
     * should therefore accept this target rather than treating the viewport
     * as empty. */
    setup(&state, GAME_AI_SCHEMER);
    state.players[0].ai_action_flags = 2;
    state.players[0].tool_inventory[6] = 1;
    state.players[0].node = 1;
    state.players[1].node = 2;
    state.players[0].relation_balance[1] = 5;
    state.land_count = 1;
    state.land_owner[0] = 1;
    state.land_level[0] = 1;
    assert(rich4_tool_set_route_targets(
        &state, 3, placeable, sizeof(placeable)));
    assert(rich4_tool_set_map_geometry(
        &state, 3, route_x, route_y, route_next, 1, land_x, land_y,
        0, NULL, NULL));
    for (seed = 1; seed < 10000; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        if (decision.has_command) break;
    }
    assert(seed < 10000 && decision.command.subject.id == 6 &&
           decision.command.target.kind == GAME_DOMAIN_PLAYER &&
           decision.command.target.id == 1 &&
           rich4_tool_inspect(&state, &decision.command) ==
               GAME_TOOL_USE_APPLIED);

    /* Moving the only typed property outside the 200x200 screen makes the
     * object set known-empty; no route raw fallback may resurrect it. */
    state.tool_land_x[0] = 200;
    saw_rejected = 0;
    for (seed = 1; seed < 10000 && !saw_rejected; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        for (i = 0; i < decision.candidate_count; i++) {
            if (decision.candidates[i].subject.kind == GAME_DOMAIN_TOOL &&
                decision.candidates[i].subject.id == 6 &&
                decision.candidates[i].result ==
                    GAME_AI_CANDIDATE_RULE_REJECTED) {
                saw_rejected = 1;
                break;
            }
        }
    }
    assert(saw_rejected && !decision.has_command);

    /* Placed route objects are normalized into the same projection and keep
     * the area gate from depending on a stale raw map-object entry. */
    state.tool_land_x[0] = 200;
    state.tool_placed_objects[0].kind = 0x10;
    state.tool_placed_objects[0].node = 2;
    state.tool_placed_objects[0].owner = 1;
    for (seed = 1; seed < 10000; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        if (decision.has_command) break;
    }
    assert(seed < 10000 && decision.command.subject.id == 6 &&
           decision.command.target.kind == GAME_DOMAIN_PLAYER &&
           decision.command.target.id == 1);

    /* Auxiliary/company records can exist in the global object table without
     * a duplicate route raw ID.  Their typed map coordinates must still make
     * the area missile's 200x200 object gate visible; an AI-owned company is
     * not a destructive target. */
    setup(&state, GAME_AI_SCHEMER);
    state.players[0].ai_action_flags = 2;
    state.players[0].tool_inventory[6] = 1;
    state.players[0].node = 1;
    state.players[1].node = 2;
    state.players[0].relation_balance[1] = 5;
    state.company_count = 1;
    state.company_owner[0] = 1;
    state.tool_company_x[0] = 32;
    state.tool_company_y[0] = 0;
    assert(rich4_tool_set_route_targets(
        &state, 3, placeable, sizeof(placeable)));
    assert(rich4_tool_set_map_geometry(
        &state, 3, route_x, route_y, route_next, 0, NULL, NULL,
        0, NULL, NULL));
    saw_rejected = 0;
    for (seed = 1; seed < 10000 && !saw_rejected; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        for (i = 0; i < decision.candidate_count; i++) {
            if (decision.candidates[i].subject.kind == GAME_DOMAIN_TOOL &&
                decision.candidates[i].subject.id == 6 &&
                decision.candidates[i].result ==
                    GAME_AI_CANDIDATE_RULE_REJECTED) {
                saw_rejected = 1;
                break;
            }
        }
    }
    assert(saw_rejected && !decision.has_command);

    state.tool_company_geometry_available = 1;
    for (seed = 1; seed < 10000; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        if (decision.has_command) break;
    }
    assert(seed < 10000 && decision.command.subject.id == 6 &&
           decision.command.target.kind == GAME_DOMAIN_PLAYER &&
           decision.command.target.id == 1 &&
           rich4_tool_inspect(&state, &decision.command) ==
               GAME_TOOL_USE_APPLIED);

    /* The same projection must not treat the acting player's company as an
     * external object merely because its raw auxiliary ID is present. */
    state.company_owner[0] = 0;
    saw_rejected = 0;
    for (seed = 1; seed < 10000 && !saw_rejected; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        for (i = 0; i < decision.candidate_count; i++) {
            if (decision.candidates[i].subject.kind == GAME_DOMAIN_TOOL &&
                decision.candidates[i].subject.id == 6 &&
                decision.candidates[i].result ==
                    GAME_AI_CANDIDATE_RULE_REJECTED) {
                saw_rejected = 1;
                break;
            }
        }
    }
    assert(saw_rejected && !decision.has_command);

    /* 0x421717 rejects any high-bit actor object in the selected 200x200
     * window before it considers ordinary external objects.  Keeping an
     * external company beside that actor catches the old implementation,
     * which skipped the actor and incorrectly accepted the area missile. */
    state.company_owner[0] = 1;
    state.route_object_raw[1] = 0x8002;
    saw_rejected = 0;
    for (seed = 1; seed < 10000 && !saw_rejected; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        for (i = 0; i < decision.candidate_count; i++) {
            if (decision.candidates[i].subject.kind == GAME_DOMAIN_TOOL &&
                decision.candidates[i].subject.id == 6 &&
                decision.candidates[i].result ==
                    GAME_AI_CANDIDATE_RULE_REJECTED) {
                saw_rejected = 1;
                break;
            }
        }
    }
    assert(saw_rejected && !decision.has_command);

    /* The local scan must inspect the whole window: an external object
     * before a later actor-owned property cannot make the target valid. */
    state.route_object_raw[1] = 0;
    state.route_object_raw[2] = 0x7d1;
    state.land_count = 1;
    state.land_owner[0] = 0;
    state.tool_land_x[0] = 32;
    state.tool_land_y[0] = 0;
    saw_rejected = 0;
    for (seed = 1; seed < 10000 && !saw_rejected; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        for (i = 0; i < decision.candidate_count; i++) {
            if (decision.candidates[i].subject.kind == GAME_DOMAIN_TOOL &&
                decision.candidates[i].subject.id == 6 &&
                decision.candidates[i].result ==
                    GAME_AI_CANDIDATE_RULE_REJECTED) {
                saw_rejected = 1;
                break;
            }
        }
    }
    assert(saw_rejected && !decision.has_command);
}

static void renderer_global_table_projection(void)
{
    GameState state;
    GameAiDecision decision;
    GameAiGlobalObjectProjection objects[2];
    GameAiGlobalProjection projection;
    uint8_t placeable[3] = {0, 1, 1};
    int16_t route_x[3] = {0, 0, 600};
    int16_t route_y[3] = {0, 0, 0};
    uint16_t route_next[3][4] = {{0}};
    uint32_t seed;
    int saw_rejected = 0;

    setup(&state, GAME_AI_SCHEMER);
    state.players[0].ai_action_flags = 2;
    state.players[0].tool_inventory[6] = 1;
    state.players[0].node = 1;
    state.players[1].node = 2;
    state.players[0].relation_balance[1] = 5;
    assert(rich4_tool_set_route_targets(
        &state, 3, placeable, sizeof(placeable)));
    assert(rich4_tool_set_map_geometry(
        &state, 3, route_x, route_y, route_next, 0, NULL, NULL,
        0, NULL, NULL));
    memset(&projection, 0, sizeof(projection));
    projection.center_world_x = 300;
    projection.center_world_y = 0;
    projection.positioned_player_mask = 3;
    projection.visible_player_mask = 3;
    projection.player_world_x[0] = 0;
    projection.player_world_x[1] = 600;
    projection.object_table_available = 1;
    projection.objects = objects;
    projection.object_count = 2;
    objects[0].raw_id = 0x8001;
    objects[0].screen_x = 120;
    objects[0].screen_y = 220;
    objects[1].raw_id = 0x8002;
    objects[1].screen_x = 320;
    objects[1].screen_y = 220;

    /* The SDL table is authoritative for 0x421717.  A target omitted from
     * that table must not be resurrected from the state's route raw IDs. */
    objects[1].raw_id = 0x8001;
    for (seed = 1; seed < 10000 && !saw_rejected; seed++) {
        int i;
        assert(rich4_ai_select_command_with_map_context_and_projection(
            &state, rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), NULL, NULL,
            &projection, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        for (i = 0; i < decision.candidate_count; i++) {
            if (decision.candidates[i].subject.kind == GAME_DOMAIN_TOOL &&
                decision.candidates[i].subject.id == 6 &&
                decision.candidates[i].result ==
                    GAME_AI_CANDIDATE_RULE_REJECTED) {
                saw_rejected = 1;
                break;
            }
        }
    }
    assert(saw_rejected && !decision.has_command);

    /* Restoring the ordinary target bit in the projected table allows the
     * same AI seed search to pass the global gate and select the missile. */
    objects[1].raw_id = 0x8002;
    for (seed = 1; seed < 10000; seed++) {
        assert(rich4_ai_select_command_with_map_context_and_projection(
            &state, rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), NULL, NULL,
            &projection, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        if (decision.has_command &&
            decision.command.type == GAME_COMMAND_USE_TOOL &&
            decision.command.subject.id == 6)
            break;
    }
    assert(seed < 10000 && decision.command.target.kind == GAME_DOMAIN_PLAYER &&
           decision.command.target.id == 1);
}

static void vehicle_tool_ai_gates(void)
{
    GameState state;
    GameAiDecision decision;
    uint32_t seed;
    int i;
    int saw_rejected;

    setup(&state, GAME_AI_SCHEMER);
    state.players[0].ai_action_flags = 2;
    state.players[0].tool_inventory[4] = 1; /* motorcycle */
    state.players[0].traffic_method = 1;
    saw_rejected = 0;
    for (seed = 1; seed < 10000 && !saw_rejected; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        for (i = 0; i < decision.candidate_count; i++) {
            if (decision.candidates[i].subject.kind == GAME_DOMAIN_TOOL &&
                decision.candidates[i].subject.id == 4 &&
                decision.candidates[i].result ==
                    GAME_AI_CANDIDATE_RULE_REJECTED) {
                saw_rejected = 1;
                break;
            }
        }
    }
    assert(saw_rejected && !decision.has_command);

    setup(&state, GAME_AI_SCHEMER);
    state.players[0].ai_action_flags = 2;
    state.players[0].tool_inventory[5] = 1; /* car */
    state.players[0].traffic_method = 2;
    saw_rejected = 0;
    for (seed = 1; seed < 10000 && !saw_rejected; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        for (i = 0; i < decision.candidate_count; i++) {
            if (decision.candidates[i].subject.kind == GAME_DOMAIN_TOOL &&
                decision.candidates[i].subject.id == 5 &&
                decision.candidates[i].result ==
                    GAME_AI_CANDIDATE_RULE_REJECTED) {
                saw_rejected = 1;
                break;
            }
        }
    }
    assert(saw_rejected && !decision.has_command);

    setup(&state, GAME_AI_SCHEMER);
    state.players[0].ai_action_flags = 2;
    state.players[0].tool_inventory[11] = 1; /* engineering vehicle */
    state.players[0].traffic_method = 3;
    saw_rejected = 0;
    for (seed = 1; seed < 10000 && !saw_rejected; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        for (i = 0; i < decision.candidate_count; i++) {
            if (decision.candidates[i].subject.kind == GAME_DOMAIN_TOOL &&
                decision.candidates[i].subject.id == 11 &&
                decision.candidates[i].result ==
                    GAME_AI_CANDIDATE_RULE_REJECTED) {
                saw_rejected = 1;
                break;
            }
        }
    }
    assert(saw_rejected && !decision.has_command);

    setup(&state, GAME_AI_SCHEMER);
    state.players[0].ai_action_flags = 2;
    state.players[0].tool_inventory[11] = 1;
    state.players[0].traffic_method = 0;
    for (seed = 1; seed < 10000; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        if (decision.has_command) break;
    }
    assert(seed < 10000 && decision.command.type == GAME_COMMAND_USE_TOOL &&
           decision.command.subject.id == 11 &&
           rich4_tool_inspect(&state, &decision.command) ==
               GAME_TOOL_USE_APPLIED);
}

static void machine_doll_route_gate(void)
{
    GameState state;
    GameAiDecision d;
    uint8_t placeable[4] = {0, 1, 1, 1};
    int16_t route_x[4] = {0, 0, 100, 200};
    int16_t route_y[4] = {0};
    uint16_t route_next[4][4] = {{0}};

    setup(&state, GAME_AI_SCHEMER);
    state.players[0].ai_action_flags = 2;
    state.players[0].tool_inventory[0] = 1;
    assert(rich4_ai_select_command(
        &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, 1, &d));
    assert(!d.has_command && has_result(&d, GAME_AI_CANDIDATE_DEFERRED_TARGET));
    state.players[0].node = 1;
    route_next[1][0] = 2;
    route_next[2][0] = 3;
    assert(rich4_tool_set_route_targets(&state, 4, placeable, sizeof(placeable)));
    assert(rich4_tool_set_map_geometry(&state, 4, route_x, route_y,
        route_next, 0, NULL, NULL, 0, NULL, NULL));
    assert(rich4_ai_select_command(
        &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, 1, &d));
    assert(!d.has_command && has_result(&d, GAME_AI_CANDIDATE_RULE_REJECTED));
    state.tool_route_flags[2] = 5u << 16;
    assert(rich4_ai_select_command(
        &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, 1, &d));
    assert(d.has_command && d.command.subject.id == 0);
    state.tool_route_next[1][1] = 3;
    assert(rich4_ai_select_command(
        &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, 1, &d));
    assert(!d.has_command);
    state.tool_route_next[1][1] = 0;
    state.tool_route_flags[2] = 17u << 16; /* Category 16. */
    state.land_count = 1;
    state.land_owner[0] = 1;
    state.land_toll_by_level[0][0] = 3000;
    state.route_object_raw[2] = 0x7d1;
    assert(rich4_ai_select_command(
        &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, 1, &d));
    assert(!d.has_command);
    state.land_toll_by_level[0][0] = 3001;
    assert(rich4_ai_select_command(
        &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, 1, &d));
    assert(d.has_command);
    state.facility_count = 1;
    state.facility_owner[0] = 1;
    state.route_object_raw[2] = 0xfa1;
    assert(rich4_ai_select_command(
        &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, 1, &d));
    assert(d.has_command); /* Fixed facility value 10000000. */
    state.tool_route_flags[2] = 27u << 16; /* Category 17. */
    state.facility_owner[0] = 0;
    assert(rich4_ai_select_command(
        &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, 1, &d));
    assert(d.has_command);
    state.facility_owner[0] = 1;
    assert(rich4_ai_select_command(
        &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, 1, &d));
    assert(!d.has_command);
}

static void tortoise_card_ai(void)
{
    GameState state;
    GameAiDecision decision;
    uint8_t placeable[4] = {0, 1, 1, 1};
    int16_t route_x[4] = {0, 0, 100, 200};
    int16_t route_y[4] = {0, 0, 0, 0};
    uint16_t route_next[4][4] = {{0}};
    uint32_t seed;
    int i;
    int saw_deferred;

    route_next[1][0] = 2;
    route_next[2][0] = 3;
    setup(&state, GAME_AI_SCHEMER);
    state.players[0].ai_action_flags = 1;
    state.players[0].card_inventory[0] = 30; /* original card ID 30 */
    state.players[0].node = 1;
    state.players[0].cash = 1000;
    state.players[0].bank = 10000;
    state.land_count = 2;
    state.land_owner[0] = -1;
    state.land_owner[1] = -1;
    state.land_price[0] = 100;
    state.land_price[1] = 150;
    state.route_object_raw[2] = 0x7d1;
    state.route_object_raw[3] = 0x7d2;
    assert(rich4_tool_set_route_targets(
        &state, 4, placeable, sizeof(placeable)));
    assert(rich4_tool_set_map_geometry(
        &state, 4, route_x, route_y, route_next, 0, NULL, NULL,
        0, NULL, NULL));

    for (seed = 1; seed < 10000; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        if (decision.has_command) break;
    }
    assert(seed < 10000 && decision.command.type == GAME_COMMAND_USE_CARD &&
           decision.command.subject.id == 29 &&
           decision.command.target.kind == GAME_DOMAIN_PLAYER &&
           decision.command.target.id == 0 &&
           rich4_card_inspect(&state, &decision.command) ==
               GAME_CARD_USE_APPLIED);

    /* The direct facility branch uses +0x24 (charge level 0) and accepts
     * level-zero facilities; it is not the current-level charge branch used
     * by the global candidate scan. */
    setup(&state, GAME_AI_SCHEMER);
    state.players[0].ai_action_flags = 1;
    state.players[0].card_inventory[0] = 30;
    state.players[0].node = 1;
    state.players[0].cash = 1000;
    state.players[0].bank = 10000;
    state.facility_count = 2;
    state.facility_owner[0] = state.facility_owner[1] = 0;
    state.facility_type[0] = state.facility_type[1] = 1;
    state.facility_level[0] = state.facility_level[1] = 0;
    state.facility_price_per_level[0] =
        state.facility_price_per_level[1] = 20000;
    state.facility_charge_by_level[0][0] =
        state.facility_charge_by_level[1][0] = 100;
    state.route_object_raw[2] = 0xfa1;
    state.route_object_raw[3] = 0xfa2;
    assert(rich4_tool_set_route_targets(
        &state, 4, placeable, sizeof(placeable)));
    assert(rich4_tool_set_map_geometry(
        &state, 4, route_x, route_y, route_next, 0, NULL, NULL,
        0, NULL, NULL));
    for (seed = 1; seed < 10000; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        if (decision.has_command) break;
    }
    assert(seed < 10000 && decision.command.type == GAME_COMMAND_USE_CARD &&
           decision.command.subject.id == 29 &&
           decision.command.target.kind == GAME_DOMAIN_PLAYER &&
           decision.command.target.id == 0);

    /* An own auxiliary/company record is skipped by the direct path; it does
     * not force the unresolved global fallback. */
    {
        int16_t company_route_x[5] = {0, 0, 100, 200, 300};
        int16_t company_route_y[5] = {0, 0, 0, 0, 0};
        uint16_t company_route_next[5][4] = {{0}};
        uint8_t company_placeable[5] = {0, 1, 1, 1, 1};
        setup(&state, GAME_AI_SCHEMER);
        state.players[0].ai_action_flags = 1;
        state.players[0].card_inventory[0] = 30;
        state.players[0].node = 1;
        state.players[0].cash = 1000;
        state.players[0].bank = 10000;
        state.land_count = 2;
        state.land_owner[0] = state.land_owner[1] = 0;
        state.land_type[0] = state.land_type[1] = 0;
        state.land_level[0] = state.land_level[1] = 1;
        state.land_price_per_level[0] = state.land_price_per_level[1] = 100;
        state.company_count = 1;
        state.company_owner[0] = 0;
        company_route_next[1][0] = 2;
        company_route_next[2][0] = 3;
        company_route_next[3][0] = 4;
        state.route_object_raw[2] = 0x7d1;
        state.route_object_raw[3] = 0x7d2;
        state.route_object_raw[4] = 0x1771;
        assert(rich4_tool_set_route_targets(
            &state, 5, company_placeable, sizeof(company_placeable)));
        assert(rich4_tool_set_map_geometry(
            &state, 5, company_route_x, company_route_y,
            company_route_next, 0, NULL, NULL, 0, NULL, NULL));
        for (seed = 1; seed < 10000; seed++) {
            assert(rich4_ai_select_command(
                &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
            if (decision.has_command) break;
        }
        assert(seed < 10000 && decision.command.type == GAME_COMMAND_USE_CARD &&
               decision.command.subject.id == 29 &&
               decision.command.target.kind == GAME_DOMAIN_PLAYER &&
               decision.command.target.id == 0);
    }

    /* A failed direct value/cash gate enters the original global table scan.
     * Without its player-mask projection, this typed state stays deferred. */
    state.players[0].cash = 100;
    saw_deferred = 0;
    for (seed = 1; seed < 10000 && !saw_deferred; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        for (i = 0; i < decision.candidate_count; i++) {
            if (decision.candidates[i].subject.kind == GAME_DOMAIN_CARD &&
                decision.candidates[i].subject.id == 29 &&
                decision.candidates[i].result ==
                    GAME_AI_CANDIDATE_DEFERRED_TARGET) {
                saw_deferred = 1;
                break;
            }
        }
    }
    assert(saw_deferred && !decision.has_command);

    state.players[0].cash = 1000;
    route_next[1][1] = 3; /* 0x40B221 branch marker: global table needed. */
    assert(rich4_tool_set_map_geometry(
        &state, 4, route_x, route_y, route_next, 0, NULL, NULL,
        0, NULL, NULL));
    saw_deferred = 0;
    for (seed = 1; seed < 10000 && !saw_deferred; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        for (i = 0; i < decision.candidate_count; i++) {
            if (decision.candidates[i].subject.kind == GAME_DOMAIN_CARD &&
                decision.candidates[i].subject.id == 29 &&
                decision.candidates[i].result ==
                    GAME_AI_CANDIDATE_DEFERRED_TARGET) {
                saw_deferred = 1;
                break;
            }
        }
    }
    assert(saw_deferred && !decision.has_command);

    /* The direct path falls back for a foreign non-type-0 land when its
     * same-name ordinary group exceeds the 1000*price-index threshold. */
    setup(&state, GAME_AI_SCHEMER);
    state.players[0].ai_action_flags = 1;
    state.players[0].card_inventory[0] = 30;
    state.players[0].node = 1;
    state.players[1].node = -1;
    state.land_count = 2;
    state.land_owner[0] = state.land_owner[1] = 1;
    state.land_type[0] = 1;
    state.land_type[1] = 0;
    state.land_group[0] = state.land_group[1] = 7;
    state.land_level[0] = state.land_level[1] = 1;
    state.land_toll_by_level[1][1] = 2000;
    state.route_object_raw[2] = 0x7d1;
    assert(rich4_tool_set_route_targets(
        &state, 4, placeable, sizeof(placeable)));
    assert(rich4_tool_set_map_geometry(
        &state, 4, route_x, route_y, route_next, 0, NULL, NULL,
        0, NULL, NULL));
    decision = select_context_card_candidate(
        &state, rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), NULL, 29);
    assert(!decision.has_command &&
           has_result(&decision, GAME_AI_CANDIDATE_DEFERRED_TARGET));

    /* A branch with a typed candidate route uses the original fallback: the
     * global ordinary-player table supplies candidate 1, whose three-node
     * route exposes two actor-owned lands above the value threshold.  This is
     * intentionally separate from the deferred branch above, which has no
     * global actor or coordinate projection to inspect. */
    setup(&state, GAME_AI_SCHEMER);
    state.players[0].ai_action_flags = 1;
    state.players[0].card_inventory[0] = 30;
    state.players[0].node = 1;
    state.players[0].cash = 20000;
    state.players[0].bank = 10000;
    state.players[1].node = 2;
    state.land_count = 2;
    state.land_owner[0] = state.land_owner[1] = 0;
    state.land_group[0] = state.land_group[1] = 7;
    state.land_level[0] = state.land_level[1] = 1;
    state.land_toll_by_level[0][1] = 6000;
    state.land_toll_by_level[1][1] = 6000;
    {
        int16_t global_route_x[5] = {0, 320, 352, 384, 416};
        int16_t global_route_y[5] = {0, 320, 320, 320, 320};
        int16_t global_land_x[2] = {320, 352};
        int16_t global_land_y[2] = {320, 320};
        uint8_t global_placeable[5] = {0, 1, 1, 1, 1};
        uint16_t global_route_next[5][4] = {{0}};
        global_route_next[1][0] = 2;
        global_route_next[1][1] = 3;
        global_route_next[2][0] = 3;
        global_route_next[3][0] = 4;
        state.route_object_raw[3] = 0x7d1;
        state.route_object_raw[4] = 0x7d2;
        assert(rich4_tool_set_route_targets(
            &state, 5, global_placeable, sizeof(global_placeable)));
        assert(rich4_tool_set_map_geometry(
            &state, 5, global_route_x, global_route_y,
            global_route_next, 2, global_land_x, global_land_y,
            0, NULL, NULL));
    }
    for (seed = 1; seed < 10000; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        if (decision.has_command) break;
    }
    assert(seed < 10000 && decision.command.type == GAME_COMMAND_USE_CARD &&
           decision.command.subject.id == 29 &&
           decision.command.target.kind == GAME_DOMAIN_PLAYER &&
           decision.command.target.id == 1 &&
           rich4_card_inspect(&state, &decision.command) ==
               GAME_CARD_USE_APPLIED);

    /* The global object table is row-major, not route-node order.  Here the
     * lower map coordinate belongs to player 2 even though player 1's route
     * record was written first; the original scan must therefore try player 2
     * first. */
    setup(&state, GAME_AI_SCHEMER);
    state.player_count = 3;
    state.players[0].ai_action_flags = 1;
    state.players[0].card_inventory[0] = 30;
    state.players[0].node = 1;
    state.players[0].cash = 20000;
    state.players[0].bank = 10000;
    state.players[1].node = 2;
    state.players[2].node = 3;
    state.players[2].play_state = 1;
    state.land_count = 2;
    state.land_owner[0] = state.land_owner[1] = 0;
    state.land_group[0] = state.land_group[1] = 7;
    state.land_level[0] = state.land_level[1] = 1;
    state.land_toll_by_level[0][1] = 6000;
    state.land_toll_by_level[1][1] = 6000;
    {
        int16_t sorted_route_x[6] = {0, 640, 100, 420, 452, 484};
        int16_t sorted_route_y[6] = {0, 640, 800, 100, 100, 100};
        int16_t sorted_land_x[2] = {452, 484};
        int16_t sorted_land_y[2] = {100, 100};
        uint8_t sorted_placeable[6] = {0, 1, 1, 1, 1, 1};
        uint16_t sorted_route_next[6][4] = {{0}};
        sorted_route_next[1][0] = 2;
        sorted_route_next[1][1] = 3;
        sorted_route_next[2][0] = 4;
        sorted_route_next[3][0] = 4;
        sorted_route_next[4][0] = 5;
        state.route_object_raw[2] = 0x8002;
        state.route_object_raw[3] = 0x8004;
        state.route_object_raw[4] = 0x7d1;
        state.route_object_raw[5] = 0x7d2;
        assert(rich4_tool_set_route_targets(
            &state, 6, sorted_placeable, sizeof(sorted_placeable)));
        assert(rich4_tool_set_map_geometry(
            &state, 6, sorted_route_x, sorted_route_y, sorted_route_next,
            2, sorted_land_x, sorted_land_y, 0, NULL, NULL));
    }
    for (seed = 1; seed < 10000; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        if (decision.has_command && decision.command.target.kind ==
                GAME_DOMAIN_PLAYER)
            break;
    }
    assert(seed < 10000 && decision.command.type == GAME_COMMAND_USE_CARD &&
           decision.command.subject.id == 29 &&
           decision.command.target.kind == GAME_DOMAIN_PLAYER &&
           decision.command.target.id == 2);

    /* The global facility branch accepts level 5 and reads its sixth charge
     * slot; the straight-path branch has a separate level < 5 check. */
    setup(&state, GAME_AI_SCHEMER);
    state.players[0].ai_action_flags = 1;
    state.players[0].card_inventory[0] = 30;
    state.players[0].node = 1;
    state.players[0].cash = 20000;
    state.players[0].bank = 10000;
    state.players[1].node = 2;
    state.facility_count = 2;
    state.facility_owner[0] = state.facility_owner[1] = 0;
    state.facility_type[0] = state.facility_type[1] = 1;
    state.facility_level[0] = state.facility_level[1] = 5;
    state.facility_charge_by_level[0][5] =
        state.facility_charge_by_level[1][5] = 9000;
    {
        int16_t level5_route_x[6] = {0, 320, 352, 384, 416, 448};
        int16_t level5_route_y[6] = {0, 320, 320, 320, 320, 320};
        int16_t level5_facility_x[2] = {416, 448};
        int16_t level5_facility_y[2] = {320, 320};
        uint8_t level5_placeable[6] = {0, 1, 1, 1, 1, 1};
        uint16_t level5_route_next[6][4] = {{0}};
        level5_route_next[1][0] = 2;
        level5_route_next[1][1] = 3;
        level5_route_next[2][0] = 4;
        level5_route_next[4][0] = 5;
        state.route_object_raw[3] = 0x8002;
        state.route_object_raw[4] = 0xfa1;
        state.route_object_raw[5] = 0xfa2;
        assert(rich4_tool_set_route_targets(
            &state, 6, level5_placeable, sizeof(level5_placeable)));
        assert(rich4_tool_set_map_geometry(
            &state, 6, level5_route_x, level5_route_y,
            level5_route_next, 0, NULL, NULL,
            2, level5_facility_x, level5_facility_y));
    }
    decision = select_context_card_candidate(
        &state, rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), NULL, 29);
    assert(decision.has_command &&
           decision.command.type == GAME_COMMAND_USE_CARD &&
           decision.command.subject.id == 29 &&
           decision.command.target.kind == GAME_DOMAIN_PLAYER &&
           decision.command.target.id == 1);

    /* 0x420970 sends a route branch to the global player-mask scan.  Property
     * coordinates alone cannot reconstruct that scan or justify a self target. */
    setup(&state, GAME_AI_SCHEMER);
    state.players[0].ai_action_flags = 1;
    state.players[0].card_inventory[0] = 30;
    state.players[0].node = 1;
    state.players[0].cash = 20000;
    state.land_count = 2;
    state.land_owner[0] = state.land_owner[1] = 0;
    state.land_level[0] = state.land_level[1] = 0;
    state.land_price_status[0] = state.land_price_status[1] = 0;
    state.land_price_per_level[0] = state.land_price_per_level[1] = 1000;
    {
        int16_t visible_route_x[4] = {0, 320, 352, 384};
        int16_t visible_route_y[4] = {0, 320, 320, 320};
        int16_t visible_land_x[2] = {320, 352};
        int16_t visible_land_y[2] = {320, 320};
        uint16_t visible_route_next[4][4] = {{0}};
        visible_route_next[1][0] = 2;
        visible_route_next[1][1] = 3;
        assert(rich4_tool_set_map_geometry(
            &state, 4, visible_route_x, visible_route_y,
            visible_route_next, 2, visible_land_x, visible_land_y,
            0, NULL, NULL));
    }
    saw_deferred = 0;
    for (seed = 1; seed < 10000 && !saw_deferred; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        for (i = 0; i < decision.candidate_count; i++) {
            if (decision.candidates[i].subject.kind == GAME_DOMAIN_CARD &&
                decision.candidates[i].subject.id == 29 &&
                decision.candidates[i].result ==
                    GAME_AI_CANDIDATE_DEFERRED_TARGET) {
                saw_deferred = 1;
                break;
            }
        }
    }
    assert(saw_deferred && !decision.has_command);

    /* Adding an unowned property still cannot turn an unavailable global
     * player-mask scan into a self-targeted command. */
    state.land_owner[1] = -1;
    saw_deferred = 0;
    for (seed = 1; seed < 10000 && !saw_deferred; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        for (i = 0; i < decision.candidate_count; i++) {
            if (decision.candidates[i].subject.kind == GAME_DOMAIN_CARD &&
                decision.candidates[i].subject.id == 29 &&
                decision.candidates[i].result ==
                    GAME_AI_CANDIDATE_DEFERRED_TARGET) {
                saw_deferred = 1;
                break;
            }
        }
    }
    assert(saw_deferred && !decision.has_command);
}

static void ai_dynamic_god_target_context(void)
{
    GameState state;
    GameAiDecision decision;
    GameAiMapTargetContext context;
    uint32_t seed;

    setup(&state, GAME_AI_SCHEMER);
    state.players[0].ai_action_flags = 1;
    state.players[0].card_inventory[0] = 23; /* 请神符 */
    context.nearest_dynamic_god =
        rich4_game_domain_ref(GAME_DOMAIN_GOD, 11);
    context.nearest_dynamic_god_type = 15;
    for (seed = 1; seed < 10000; seed++) {
        assert(rich4_ai_select_command_with_map_context(
            &state, rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), NULL,
            &context, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        if (decision.has_command) break;
    }
    assert(seed < 10000 && decision.command.type == GAME_COMMAND_USE_CARD &&
           decision.command.subject.id == 22 &&
           decision.command.target.kind == GAME_DOMAIN_GOD &&
           decision.command.target.id == 11 && decision.command.value == 15);

    /* The legacy no-context entry point remains explicitly deferred. */
    for (seed = 1; seed < 10000; seed++) {
        assert(rich4_ai_select_command(
            &state, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        if (has_result(&decision, GAME_AI_CANDIDATE_DEFERRED_TARGET)) break;
    }
    assert(seed < 10000 && !decision.has_command);
}

static void original_ai_noop_cards_are_rejected(void)
{
    GameState state;
    GameAiDecision decision;
    int i;
    int saw_card5 = 0;
    int saw_card6 = 0;

    /* The original card-AI table maps both card IDs 5 and 6 to
     * 0x41E6E3 (xor eax,eax; ret).  These are not the same as the
     * resource-independent target-table cases that remain deferred. */
    setup(&state, GAME_AI_SCHEMER);
    state.players[0].card_inventory[0] = 5;
    decision = select_context_card_candidate(
        &state, rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), NULL, 4);
    assert(!decision.has_command);
    for (i = 0; i < decision.candidate_count; i++)
        if (decision.candidates[i].subject.kind == GAME_DOMAIN_CARD &&
            decision.candidates[i].subject.id == 4) {
            assert(decision.candidates[i].result ==
                   GAME_AI_CANDIDATE_RULE_REJECTED);
            saw_card5 = 1;
        }
    assert(saw_card5);

    setup(&state, GAME_AI_SCHEMER);
    state.players[0].card_inventory[0] = 6;
    decision = select_context_card_candidate(
        &state, rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), NULL, 5);
    assert(!decision.has_command);
    for (i = 0; i < decision.candidate_count; i++)
        if (decision.candidates[i].subject.kind == GAME_DOMAIN_CARD &&
            decision.candidates[i].subject.id == 5) {
            assert(decision.candidates[i].result ==
                   GAME_AI_CANDIDATE_RULE_REJECTED);
            saw_card6 = 1;
        }
    assert(saw_card6);
    {
        static const int noops[] = {5, 6, 18, 19, 20, 21};
        unsigned int n;
        for (n = 0; n < sizeof(noops) / sizeof(noops[0]); n++) {
            setup(&state, GAME_AI_SCHEMER);
            state.players[0].card_inventory[0] = noops[n];
            state.players[0].card_inventory[1] = 22;
            state.players[0].god_info = 9;
            state.players[0].god_type = 7;
            decision = select_context_card(&state,
                rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), NULL, 21);
            assert(decision.has_command && decision.command.subject.id == 21);
            assert(decision.candidate_count == 2 &&
                decision.candidates[0].result == GAME_AI_CANDIDATE_RULE_REJECTED);
        }
    }
}

static void stock_buy_sell_and_closed(void)
{
    GameState state, before;
    GameAiDecision d;
    uint32_t seed;
    int i;
    setup(&state, GAME_AI_ORDINARY);
    state.players[0].bank = 100000;
    state.players[0].ai_stock_ratio = 50;
    state.stocks[0].f10 = 100;
    state.stocks[0].f12 = 10.0f;
    state.stocks[0].f16 = 8.0f;
    state.stocks[0].f20 = 5.0f;
    state.stocks[0].f28 = 3.0f;
    for (i = 0; i < 24; i++) state.stock_history[0][i] = i < 18 ? 10.0f : 20.0f;
    state.stock_history_cursor = 24;
    before = state;
    for (seed = 1; seed < 10000; seed++) {
        assert(rich4_ai_select_command(&state, 0, GAME_AI_PHASE_STOCK_BUY,
                                       seed, &d));
        if (d.has_command) break;
    }
    assert(seed < 10000 && d.command.type == GAME_COMMAND_BUY_STOCK &&
           d.command.subject.id == 0 && d.command.value > 0);
    assert(memcmp(&state, &before, sizeof(state)) == 0);
    state.stock_market_closed = 1;
    assert(rich4_ai_select_command(&state, 0, GAME_AI_PHASE_STOCK_BUY,
                                   seed, &d));
    assert(!d.has_command && has_result(&d, GAME_AI_CANDIDATE_MARKET_CLOSED));

    setup(&state, GAME_AI_ORDINARY);
    state.players[0].loan = 1000;
    state.players[0].loan_due_date = state.elapsed_days + 6;
    state.player_stock_amount[0][0] = 20;
    state.player_stock_average_cost[0][0] = 10.0f;
    state.stocks[0].f20 = 20.0f;
    state.stocks[0].f16 = 21.0f;
    state.stocks[0].f28 = 0.5f;
    state.stock_history[0][0] = 5.0f;
    assert(rich4_ai_select_command(&state, 0, GAME_AI_PHASE_STOCK_SELL,
                                   7, &d));
    assert(d.has_command && d.command.type == GAME_COMMAND_SELL_STOCK &&
           d.command.value == 20 && d.next_phase == GAME_AI_PHASE_STOCK_SELL);
}

static void stock_buy_truncation(void)
{
    GameState state, before;
    GameAiDecision d;
    uint32_t seed;
    int i, scenario;
    /* Quantity half/above-half boundaries, then two fractional holdings:
     * 0x42BFF1 truncates each addition, not the final portfolio sum. */
    const int banks[] = {40007, 40007, 40000, 16777218};
    const float quotes[] = {2.0f, 4.0f, 1.0f, 0.5f};
    const int quantities[] = {10001, 5000, 20000, 2};
    for (scenario = 0; scenario < 4; scenario++) {
        setup(&state, GAME_AI_ORDINARY);
        state.players[0].bank = banks[scenario];
        state.players[0].ai_stock_ratio = 50;
        state.stocks[0].f10 = 30000;
        state.stocks[0].f12 = 10.0f;
        state.stocks[0].f20 = quotes[scenario];
        state.stocks[0].f16 = quotes[scenario];
        state.stocks[0].f28 = 3.0f;
        state.stock_history_cursor = 24;
        for (i = 0; i < 24; i++)
            state.stock_history[0][i] = i < 18 ? 10.0f : 20.0f;
        if (scenario == 2) {
            state.player_stock_amount[0][1] = 1;
            state.player_stock_amount[0][2] = 1;
            state.stocks[1].f20 = state.stocks[2].f20 = 0.75f;
        }
        if (scenario == 3) {
            /* The previous integer passes through single precision:
             * 16777217 becomes 16777216 before the third addition. */
            state.player_stock_amount[0][1] = 1;
            state.player_stock_amount[0][2] = 1;
            state.player_stock_amount[0][3] = 1;
            state.stocks[1].f20 = 16777216.0f;
            state.stocks[2].f20 = 1.0f;
            state.stocks[3].f20 = 0.75f;
        }
        before = state;
        for (seed = 1; seed < 10000; seed++) {
            assert(rich4_ai_select_command(&state, 0, GAME_AI_PHASE_STOCK_BUY,
                                           seed, &d));
            if (d.has_command) break;
        }
        assert(seed < 10000 && d.command.type == GAME_COMMAND_BUY_STOCK);
        assert(d.command.subject.id == 0 && d.command.value == quantities[scenario]);
        assert(memcmp(&state, &before, sizeof(state)) == 0);
    }
}

static void company_stock_buy_and_loan_gate(void)
{
    GameState state, before;
    GameAiDecision d;
    uint32_t seed;
    int found = 0;
    int i;

    setup(&state, GAME_AI_ORDINARY);
    state.players[0].bank = 100000;
    state.players[0].ai_stock_ratio = 50;
    state.stocks[0].f4 = 1;
    state.stocks[0].f8 = 100;
    state.stocks[0].f10 = 100;
    state.stocks[0].f20 = 5.0f;
    state.stocks[0].f16 = 5.0f;
    state.company_count = 1;
    state.stock_company_auxiliary[0] = 0;
    state.stock_company_value[0] = 100000;
    state.stock_company_available[0] = 200;
    state.company_owner[0] = -1;
    state.company_period_business[0] = 1;
    state.company_total_business[0] = 15000;
    state.calendar_start_year = 2026;
    state.calendar_start_month = 8;
    state.calendar_start_day = 9;
    state.elapsed_days = 54; /* two completed original months */
    before = state;
    for (seed = 1; seed < 10000; seed++) {
        assert(rich4_ai_select_command(&state, 0, GAME_AI_PHASE_STOCK_BUY,
                                       seed, &d));
        if (d.has_command) break;
    }
    assert(seed < 10000 && d.command.type == GAME_COMMAND_BUY_STOCK &&
           d.command.subject.id == 0 && d.command.value == 100);
    for (i = 0; i < d.candidate_count; i++) {
        if (d.candidates[i].subject.kind == GAME_DOMAIN_STOCK &&
            d.candidates[i].subject.id == 0 &&
            d.candidates[i].result == GAME_AI_CANDIDATE_ELIGIBLE) {
            assert(d.candidates[i].score == 11);
            found = 1;
        }
    }
    assert(found && memcmp(&state, &before, sizeof(state)) == 0);

    state.company_owner[0] = 1;
    state.player_stock_amount[1][0] = 250;
    assert(rich4_ai_select_command(&state, 0, GAME_AI_PHASE_STOCK_BUY,
                                   seed, &d));
    found = 0;
    for (i = 0; i < d.candidate_count; i++) {
        if (d.candidates[i].subject.kind == GAME_DOMAIN_STOCK &&
            d.candidates[i].subject.id == 0 &&
            d.candidates[i].result == GAME_AI_CANDIDATE_ELIGIBLE) {
            assert(d.candidates[i].score == 12);
            found = 1;
        }
    }
    assert(found && d.has_command);

    state.players[0].loan_due_date = state.elapsed_days + 14;
    assert(rich4_ai_select_command(&state, 0, GAME_AI_PHASE_STOCK_BUY,
                                   seed, &d));
    assert(!d.has_command);
}

static void company_stock_sell(void)
{
    GameState state;
    GameAiDecision d;
    uint32_t seed;
    int found = 0;
    int i;

    setup(&state, GAME_AI_ORDINARY);
    state.players[0].bank = 1000;
    state.player_stock_amount[0][0] = 20;
    state.player_stock_average_cost[0][0] = 5.0f;
    state.player_stock_amount[1][0] = 100;
    state.stocks[0].f4 = 1;
    state.stocks[0].f8 = 10;
    state.stocks[0].f10 = 100;
    state.stocks[0].f16 = 11.0f;
    state.stocks[0].f20 = 10.5f;
    state.company_count = 1;
    state.stock_company_auxiliary[0] = 0;
    state.stock_company_value[0] = 50000;
    state.stock_company_available[0] = 10;
    state.company_owner[0] = 1;
    state.company_period_business[0] = 0;
    state.company_total_business[0] = 5000;

    for (seed = 1; seed < 10000; seed++) {
        assert(rich4_ai_select_command(&state, 0, GAME_AI_PHASE_STOCK_SELL,
                                       seed, &d));
        if (d.has_command) break;
    }
    assert(seed < 10000 && d.command.type == GAME_COMMAND_SELL_STOCK &&
           d.command.subject.id == 0 && d.command.value == 20);
    for (i = 0; i < d.candidate_count; i++) {
        if (d.candidates[i].subject.kind == GAME_DOMAIN_STOCK &&
            d.candidates[i].subject.id == 0 &&
            d.candidates[i].result == GAME_AI_CANDIDATE_ELIGIBLE) {
            assert(d.candidates[i].score == 3);
            found = 1;
        }
    }
    assert(found);
}

static void independent_stock_sell_thresholds(void)
{
    GameState state;
    GameAiDecision d;
    uint32_t seed;
    int i;

    setup(&state, GAME_AI_ORDINARY);
    state.player_stock_amount[0][0] = 10;
    state.player_stock_average_cost[0][0] = 10.0f;
    state.stocks[0].f20 = 10.0f;
    state.stocks[0].f16 = 10.5f;
    state.stocks[0].f28 = 2.0f;
    state.players[0].cash = 10000;
    for (seed = 1; seed < 10000; seed++) {
        assert(rich4_ai_select_command(&state, 0, GAME_AI_PHASE_STOCK_SELL,
                                       seed, &d));
        if (d.has_command) break;
    }
    assert(seed < 10000 && d.has_command);
    for (i = 0; i < d.candidate_count; i++) {
        if (d.candidates[i].subject.kind == GAME_DOMAIN_STOCK &&
            d.candidates[i].subject.id == 0 &&
            d.candidates[i].result == GAME_AI_CANDIDATE_ELIGIBLE)
            assert(d.candidates[i].score == 6);
    }

    /* 0x457DBC truncates: ratio 2.9 contributes trunc(2.8)=2;
     * each liquidity term contributes trunc(6.8)=6. */
    state.stocks[0].f20 = 29.0f;
    state.stocks[0].f16 = 30.0f;
    state.stocks[0].f28 = 0.0f;
    for (seed = 1; seed < 10000; seed++) {
        assert(rich4_ai_select_command(&state, 0, GAME_AI_PHASE_STOCK_SELL,
                                       seed, &d));
        if (d.has_command) break;
    }
    assert(seed < 10000 && d.has_command);
    for (i = 0; i < d.candidate_count; i++) {
        if (d.candidates[i].subject.kind == GAME_DOMAIN_STOCK &&
            d.candidates[i].subject.id == 0 &&
            d.candidates[i].result == GAME_AI_CANDIDATE_ELIGIBLE)
            assert(d.candidates[i].score == 16);
    }

    state.stocks[0].f20 = 10.0f;
    state.stocks[0].f16 = 20.0f;
    state.stocks[0].f28 = 2.0f;
    state.players[0].loan = 100000;
    state.players[0].loan_due_date = state.elapsed_days + 6;
    state.players[0].cash = 0;
    assert(rich4_ai_select_command(&state, 0, GAME_AI_PHASE_STOCK_SELL,
                                   seed, &d));
    for (i = 0; i < d.candidate_count; i++) {
        if (d.candidates[i].subject.kind == GAME_DOMAIN_STOCK &&
            d.candidates[i].subject.id == 0 &&
            d.candidates[i].result == GAME_AI_CANDIDATE_ELIGIBLE)
            assert(d.candidates[i].score == 12);
    }
}

static void upgrade_move_and_empty(void)
{
    GameState state;
    GameAiDecision d;
    setup(&state, GAME_AI_SCHEMER);
    state.land_count = 2;
    state.land_owner[0] = 0;
    state.land_owner[1] = 0;
    state.land_level[0] = 1;
    state.land_level[1] = 2;
    state.land_toll_by_level[0][1] = 50;
    state.land_toll_by_level[1][2] = 100;
    assert(rich4_ai_select_command(&state, 0, GAME_AI_PHASE_PROPERTY_UPGRADE,
                                   1, &d));
    assert(d.has_command && d.command.type == GAME_COMMAND_UPGRADE_PROPERTY &&
           d.command.subject.kind == GAME_DOMAIN_LAND && d.command.subject.id == 1);

    /* 0x40B455 checks the land subtype, not the parity overlay.  A special
     * land is skipped, while a normal land with a nonzero overlay remains a
     * valid high-value upgrade candidate. */
    state.land_type[0] = 1;
    state.land_level[0] = 1;
    state.land_toll_by_level[0][1] = 1000;
    state.land_price_status[1] = 1;
    state.land_toll_by_level[1][2] = 100;
    assert(rich4_ai_select_command(&state, 0, GAME_AI_PHASE_PROPERTY_UPGRADE,
                                   1, &d));
    assert(d.has_command && d.command.subject.kind == GAME_DOMAIN_LAND &&
           d.command.subject.id == 1);

    /* The facility half scores the record's +0x22 base value and accepts a
     * level-zero record when its subtype's maximum allows it. */
    memset(&state, 0, sizeof(state));
    setup(&state, GAME_AI_SCHEMER);
    state.facility_count = 1;
    state.facility_owner[0] = 0;
    state.facility_type[0] = 1;
    state.facility_level[0] = 0;
    state.facility_price[0] = 123;
    assert(rich4_ai_select_command(&state, 0, GAME_AI_PHASE_PROPERTY_UPGRADE,
                                   1, &d));
    assert(d.has_command && d.command.subject.kind == GAME_DOMAIN_FACILITY &&
           d.command.subject.id == 0);
    assert(rich4_ai_select_command(&state, 0, GAME_AI_PHASE_MOVE, 1, &d));
    assert(d.has_command && d.command.type == GAME_COMMAND_ROLL_DICE &&
           d.next_phase == GAME_AI_PHASE_DONE);
    memset(state.players[0].card_inventory, 0,
           sizeof(state.players[0].card_inventory));
    memset(state.players[0].tool_inventory, 0,
           sizeof(state.players[0].tool_inventory));
    assert(rich4_ai_select_command(&state, 0, GAME_AI_PHASE_CARD_OR_TOOL,
                                   1, &d));
    assert(!d.has_command);
}

static void shared_purchase_reserve_policy(void)
{
    GameState state;
    setup(&state, GAME_AI_ORDINARY);
    state.initial_fund = 100000;
    state.price_index = 2;
    state.players[0].cash = 6000;
    state.players[0].bank = 5000;
    assert(rich4_ai_purchase_allowed(&state, 0, 500));
    assert(!rich4_ai_purchase_allowed(&state, 0, 1500));
    /* 0x41D7E6 truncates 199 * .05 to 9; strict reserve boundary. */
    state.initial_fund = 199;
    state.price_index = 2;
    state.players[0].cash = 100;
    state.players[0].bank = 0;
    assert(rich4_ai_purchase_allowed(&state, 0, 81));
    assert(!rich4_ai_purchase_allowed(&state, 0, 82));
    state.initial_fund = -1;
    assert(!rich4_ai_purchase_allowed(&state, 0, 1));
}

static void original_card_target_strategies(void)
{
    GameState state;
    GameDomainRef current_property;
    uint8_t player_on_map[GAME_MAX_PLAYERS];
    GameAiDecision decision;
    uint32_t seed;
    int i;
    int saw_winter_sleep_selected = 0;
    int saw_winter_sleep_random_gate = 0;

    /* 0x41FF77: send-god is only eligible for a sendable god, or for an
     * attached timed bomb whose original +4 countdown is below 13. */
    setup(&state, GAME_AI_SCHEMER);
    state.players[0].card_inventory[0] = 22; /* 送神符 */
    state.players[0].attached_timed_bomb_slot = 0;
    state.tool_placed_objects[0].kind = 0x12;
    state.tool_placed_objects[0].attached_player = 1;
    state.tool_placed_objects[0].countdown = 12;
    decision = select_context_card(&state,
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), NULL, 21);
    assert(decision.command.subject.id == 21 &&
           rich4_game_domain_ref_is_none(decision.command.target));

    state.tool_placed_objects[0].countdown = 13;
    decision = select_context_card_candidate(
        &state, rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), NULL, 21);
    for (i = 0; i < decision.candidate_count; i++)
        if (decision.candidates[i].subject.id == 21)
            assert(decision.candidates[i].result ==
                   GAME_AI_CANDIDATE_RULE_REJECTED);

    setup(&state, GAME_AI_SCHEMER);
    state.players[0].card_inventory[0] = 22; /* 送神符 */
    state.players[0].god_info = 1;
    state.players[0].god_type = 5;
    decision = select_context_card(&state,
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), NULL, 21);
    assert(decision.command.subject.id == 21);

    setup(&state, GAME_AI_SCHEMER);
    state.players[0].card_inventory[0] = 22; /* 送神符 */
    decision = select_context_card_candidate(
        &state, rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), NULL, 21);
    for (i = 0; i < decision.candidate_count; i++)
        if (decision.candidates[i].subject.id == 21)
            assert(decision.candidates[i].result ==
                   GAME_AI_CANDIDATE_RULE_REJECTED);

    /* 0x41E779: the relation-preferred visible player is accepted when
     * richer than twice the actor and below 30,000 * price_index. */
    setup(&state, GAME_AI_SCHEMER);
    memset(player_on_map, 0, sizeof(player_on_map));
    state.players[0].card_inventory[0] = 2;
    state.players[0].cash = 1000;
    state.players[1].cash = 2500;
    state.players[0].relation_balance[1] = 10;
    player_on_map[1] = 1;
    decision = select_context_card(&state,
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), player_on_map, 1);
    assert(decision.command.target.kind == GAME_DOMAIN_PLAYER &&
           decision.command.target.id == 1);

    /* The fallback scan uses the 50,000 * price_index / three-times
     * thresholds and keeps the first visible matching player. */
    setup(&state, GAME_AI_SCHEMER);
    state.player_count = 3;
    memset(player_on_map, 0, sizeof(player_on_map));
    state.players[0].card_inventory[0] = 2;
    state.players[0].cash = 1000;
    state.players[1].cash = 3500;
    state.players[2].cash = 2000;
    player_on_map[1] = 1;
    player_on_map[2] = 1;
    decision = select_context_card(&state,
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), player_on_map, 1);
    assert(decision.command.target.kind == GAME_DOMAIN_PLAYER &&
           decision.command.target.id == 1);

    setup(&state, GAME_AI_GOOD_CHILD);
    memset(player_on_map, 0, sizeof(player_on_map));
    current_property = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
    state.players[0].card_inventory[0] = 7;
    state.land_count = 1;
    state.land_owner[0] = 0;
    state.land_level[0] = 1;
    state.land_type[0] = 0;
    current_property =
        rich4_game_domain_ref(GAME_DOMAIN_LAND, 0);
    decision = select_context_card(&state, current_property, player_on_map, 6);
    assert(decision.command.target.kind == GAME_DOMAIN_LAND &&
           decision.command.target.id == 0);

    /* 0x41EAE2: exchange-land AI uses an actor-owned level 0/1 source and
     * selects a more expensive, more developed typed land target. */
    setup(&state, GAME_AI_SCHEMER);
    memset(player_on_map, 0, sizeof(player_on_map));
    state.players[0].card_inventory[0] = 4; /* 换地 */
    state.players[0].relation_balance[1] = 10;
    state.land_count = 2;
    state.land_owner[0] = 0;
    state.land_owner[1] = 1;
    state.land_group[0] = 1;
    state.land_group[1] = 2;
    state.land_type[0] = state.land_type[1] = 0;
    state.land_level[0] = 1;
    state.land_level[1] = 2;
    state.land_price[0] = 100;
    state.land_price[1] = 200;
    current_property = rich4_game_domain_ref(GAME_DOMAIN_LAND, 0);
    decision = select_context_card(&state, current_property, player_on_map, 3);
    assert(decision.command.subject.kind == GAME_DOMAIN_CARD &&
           decision.command.subject.id == 3 &&
           decision.command.target.kind == GAME_DOMAIN_LAND &&
           decision.command.target.id == 1 &&
           decision.command.value == 0 &&
           (decision.command.flags & GAME_CARD_FLAG_SECONDARY_FACILITY) == 0);

    /* With property coordinates available, the original visible-object
     * table is authoritative for exchange targets: an otherwise eligible
     * property outside the viewport must not be selected. */
    {
        int16_t route_x[2] = {0, 320};
        int16_t route_y[2] = {0, 320};
        uint16_t route_next[2][4] = {{0}};
        int16_t land_x[2] = {320, 1000};
        int16_t land_y[2] = {320, 1000};

        setup(&state, GAME_AI_SCHEMER);
        memset(player_on_map, 0, sizeof(player_on_map));
        state.players[0].node = 1;
        state.players[0].card_inventory[0] = 4; /* 换地 */
        state.players[0].relation_balance[1] = 10;
        state.land_count = 2;
        state.land_owner[0] = 0;
        state.land_owner[1] = 1;
        state.land_group[0] = 1;
        state.land_group[1] = 2;
        state.land_type[0] = state.land_type[1] = 0;
        state.land_level[0] = 1;
        state.land_level[1] = 2;
        state.land_price[0] = 100;
        state.land_price[1] = 200;
        current_property = rich4_game_domain_ref(GAME_DOMAIN_LAND, 0);
        assert(rich4_tool_set_map_geometry(
            &state, 2, route_x, route_y, route_next, 2, land_x, land_y,
            0, NULL, NULL));
        decision = select_context_card_candidate(
            &state, current_property, player_on_map, 3);
        for (i = 0; i < decision.candidate_count; i++)
            if (decision.candidates[i].subject.id == 3)
                assert(decision.candidates[i].result ==
                       GAME_AI_CANDIDATE_RULE_REJECTED);

        land_x[1] = 352;
        land_y[1] = 320;
        assert(rich4_tool_set_map_geometry(
            &state, 2, route_x, route_y, route_next, 2, land_x, land_y,
            0, NULL, NULL));
        decision = select_context_card(&state, current_property,
                                       player_on_map, 3);
        assert(decision.command.target.kind == GAME_DOMAIN_LAND &&
               decision.command.target.id == 1);
    }

    /* 0x41ED3E does not inspect level on the nonzero-type land branch. */
    setup(&state, GAME_AI_GOOD_CHILD);
    memset(player_on_map, 0, sizeof(player_on_map));
    current_property = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
    state.players[0].card_inventory[0] = 7;
    state.land_count = 2;
    state.land_owner[0] = state.land_owner[1] = 0;
    state.land_group[0] = state.land_group[1] = 3;
    state.land_type[0] = 1;
    state.land_level[0] = 0;
    current_property =
        rich4_game_domain_ref(GAME_DOMAIN_LAND, 0);
    decision = select_context_card(&state, current_property, player_on_map, 6);
    assert(decision.command.target.kind == GAME_DOMAIN_LAND &&
           decision.command.target.id == 0);

    setup(&state, GAME_AI_SCHEMER);
    memset(player_on_map, 0, sizeof(player_on_map));
    current_property = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
    state.players[0].card_inventory[0] = 11;
    state.players[0].relation_balance[1] = 10;
    state.land_count = 1;
    state.land_owner[0] = 1;
    state.land_level[0] = 5;
    state.land_price[0] = 500;
    state.facility_count = 1;
    state.facility_owner[0] = 1;
    state.facility_level[0] = 3;
    state.facility_price[0] = 100;
    decision = select_context_card(&state, current_property, player_on_map, 10);
    assert(decision.command.target.kind == GAME_DOMAIN_FACILITY &&
           decision.command.target.id == 0);

    /* 0x41F400 also scans the visible object table.  A high-level foreign
     * property outside the projected viewport is not a monster-card target;
     * moving it into the viewport makes the same typed branch eligible. */
    {
        int16_t route_x[2] = {0, 320};
        int16_t route_y[2] = {0, 320};
        uint16_t route_next[2][4] = {{0}};
        int16_t land_x[1] = {1000};
        int16_t land_y[1] = {1000};

        setup(&state, GAME_AI_SCHEMER);
        memset(player_on_map, 0, sizeof(player_on_map));
        state.players[0].node = 1;
        state.players[0].card_inventory[0] = 12; /* 拆除 */
        state.players[0].relation_balance[1] = 10;
        state.land_count = 1;
        state.land_owner[0] = 1;
        state.land_level[0] = 4;
        current_property = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
        assert(rich4_tool_set_map_geometry(
            &state, 2, route_x, route_y, route_next, 1, land_x, land_y,
            0, NULL, NULL));
        decision = select_context_card_candidate(
            &state, current_property, player_on_map, 11);
        for (i = 0; i < decision.candidate_count; i++)
            if (decision.candidates[i].subject.id == 11)
                assert(decision.candidates[i].result ==
                       GAME_AI_CANDIDATE_RULE_REJECTED);

        land_x[0] = 352;
        land_y[0] = 320;
        assert(rich4_tool_set_map_geometry(
            &state, 2, route_x, route_y, route_next, 1, land_x, land_y,
            0, NULL, NULL));
        decision = select_context_card(&state, current_property,
                                       player_on_map, 11);
        assert(decision.command.target.kind == GAME_DOMAIN_LAND &&
               decision.command.target.id == 0);
    }

    /* 0x41F6A9: demolition first reuses the monster-card property selector;
     * only its typed card subject changes. */
    setup(&state, GAME_AI_SCHEMER);
    memset(player_on_map, 0, sizeof(player_on_map));
    current_property = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
    state.players[0].card_inventory[0] = 12; /* 拆除 */
    state.players[0].relation_balance[1] = 10;
    state.land_count = 1;
    state.land_owner[0] = 1;
    state.land_level[0] = 4;
    state.land_price[0] = 500;
    decision = select_context_card(&state, current_property, player_on_map, 11);
    assert(decision.command.subject.kind == GAME_DOMAIN_CARD &&
           decision.command.subject.id == 11 &&
           decision.command.target.kind == GAME_DOMAIN_LAND &&
           decision.command.target.id == 0);

    /* The same original handler then accepts a type-16 roadblock on a
     * foreign property.  The route raw record supplies the owner projection
     * used by 0x48B8C4; the emitted command now uses the typed object slot. */
    setup(&state, GAME_AI_SCHEMER);
    memset(player_on_map, 0, sizeof(player_on_map));
    current_property = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
    state.players[0].card_inventory[0] = 12; /* 拆除 */
    state.tool_route_node_count = 2;
    state.route_object_raw[1] = 0x7d1;
    state.land_count = 1;
    state.land_owner[0] = 1;
    state.land_level[0] = 1;
    state.tool_placed_objects[0].kind = 0x10;
    state.tool_placed_objects[0].node = 1;
    decision = select_context_card(&state, current_property,
                                   player_on_map, 11);
    assert(decision.command.subject.id == 11 &&
           decision.command.target.kind == GAME_DOMAIN_MAP_OBJECT &&
           decision.command.target.id == 0);

    /* Type 17 is the complementary original branch: only a mine on the
     * acting player's property is selected. */
    state.tool_placed_objects[0].kind = 0x11;
    state.land_owner[0] = 0;
    decision = select_context_card(&state, current_property,
                                   player_on_map, 11);
    assert(decision.command.target.kind == GAME_DOMAIN_MAP_OBJECT &&
           decision.command.target.id == 0);

    /* 0x41F6A9 only accepts dynamic types 16 and 17; the adjacent type-18
     * timed bomb is deliberately not a candidate in this AI handler. */
    state.tool_placed_objects[0].kind = 0x12;
    decision = select_context_card_candidate(&state, current_property,
                                             player_on_map, 11);
    assert(!decision.has_command &&
           has_result(&decision, GAME_AI_CANDIDATE_RULE_REJECTED));
    state.land_owner[0] = 1;
    decision = select_context_card_candidate(&state, current_property,
                                             player_on_map, 11);
    for (i = 0; i < decision.candidate_count; i++)
        if (decision.candidates[i].subject.id == 11)
            assert(decision.candidates[i].result ==
                   GAME_AI_CANDIDATE_RULE_REJECTED);

    setup(&state, GAME_AI_SCHEMER);
    memset(player_on_map, 0, sizeof(player_on_map));
    current_property = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
    state.players[0].card_inventory[0] = 13;
    state.players[0].relation_balance[1] = 10;
    state.players[1].card_inventory[0] = 10; /* f7=2, price=180 */
    player_on_map[1] = 1;
    decision = select_context_card(&state, current_property, player_on_map, 12);
    assert(decision.command.target.kind == GAME_DOMAIN_PLAYER &&
           decision.command.target.id == 1 &&
           decision.command.value == 9 &&
           (decision.command.flags & GAME_CARD_FLAG_ROB_TOOL) == 0);

    /* 0x41FE6F: dream-walk and frame cards prefer the relation target, but
     * skip sleeping players and players carrying the original card 18
     * (revenge).  The remaining visible player is selected as the typed
     * player-target subset of the original visibility table. */
    setup(&state, GAME_AI_SCHEMER);
    state.player_count = 3;
    state.players[0].card_inventory[0] = 16; /* 梦游 */
    state.players[0].relation_balance[1] = 10;
    state.players[1].play_state = 1;
    state.players[2].play_state = 1;
    memset(player_on_map, 0, sizeof(player_on_map));
    player_on_map[1] = 1;
    player_on_map[2] = 1;
    decision = select_context_card(&state,
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), player_on_map, 15);
    assert(decision.command.target.kind == GAME_DOMAIN_PLAYER &&
           decision.command.target.id == 1);

    state.players[1].days_sleeping = 1;
    state.players[1].card_inventory[0] = 18; /* 复仇 */
    decision = select_context_card(&state,
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), player_on_map, 15);
    assert(decision.command.target.kind == GAME_DOMAIN_PLAYER &&
           decision.command.target.id == 2);

    setup(&state, GAME_AI_SCHEMER);
    state.players[0].card_inventory[0] = 17; /* 陷害 */
    memset(player_on_map, 0, sizeof(player_on_map));
    player_on_map[1] = 1;
    decision = select_context_card(&state,
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), player_on_map, 16);
    assert(decision.command.target.kind == GAME_DOMAIN_PLAYER &&
           decision.command.target.id == 1);

    /* 0x41FE6F tests the raw object high bit only to distinguish a player
     * object and consumes the low ordinary-player mask. A typed special
     * route node alone cannot become a special-player target. */
    setup(&state, GAME_AI_SCHEMER);
    state.players[0].card_inventory[0] = 16; /* 梦游 */
    state.special_player_f10[0] = 0;
    state.special_player_node[0] = 1;
    {
        int16_t route_x[2] = {0, 100};
        int16_t route_y[2] = {0, 0};
        uint16_t route_next[2][4] = {{0}};
        assert(rich4_tool_set_map_geometry(
            &state, 2, route_x, route_y, route_next, 0, NULL, NULL,
            0, NULL, NULL));
    }
    memset(player_on_map, 0, sizeof(player_on_map));
    decision = select_context_card_candidate(
        &state, rich4_game_domain_ref(GAME_DOMAIN_NONE, -1),
        player_on_map, 15);
    assert(!decision.has_command &&
           has_result(&decision, GAME_AI_CANDIDATE_RULE_REJECTED));

    setup(&state, GAME_AI_SCHEMER);
    state.players[0].card_inventory[0] = 17; /* 陷害 */
    state.special_player_f10[0] = 0;
    state.special_player_node[0] = 1;
    {
        int16_t route_x[2] = {0, 100};
        int16_t route_y[2] = {0, 0};
        uint16_t route_next[2][4] = {{0}};
        assert(rich4_tool_set_map_geometry(
            &state, 2, route_x, route_y, route_next, 0, NULL, NULL,
            0, NULL, NULL));
    }
    memset(player_on_map, 0, sizeof(player_on_map));
    decision = select_context_card_candidate(
        &state, rich4_game_domain_ref(GAME_DOMAIN_NONE, -1),
        player_on_map, 16);
    assert(!decision.has_command &&
           has_result(&decision, GAME_AI_CANDIDATE_RULE_REJECTED));

    /* 0x41FACC: stay first accepts self on a qualifying owned land/facility,
     * then scans visible players standing on an owned qualifying facility. */
    setup(&state, GAME_AI_SCHEMER);
    state.players[0].card_inventory[0] = 14; /* 停留 */
    state.players[0].cash = 11000;
    state.land_count = 1;
    state.land_owner[0] = 0;
    state.land_type[0] = 0;
    state.land_price_status[0] = 1; /* 0x41FACC checks type, not parity/status. */
    state.land_level[0] = 2;
    state.land_price_per_level[0] = 100;
    current_property = rich4_game_domain_ref(GAME_DOMAIN_LAND, 0);
    decision = select_context_card(&state, current_property, player_on_map, 13);
    assert(decision.command.target.kind == GAME_DOMAIN_NONE);
    assert(decision.selection_after.card_target_raw == 0);

    setup(&state, GAME_AI_SCHEMER);
    state.players[0].card_inventory[0] = 14; /* 停留 */
    state.players[1].node = 1;
    memset(player_on_map, 0, sizeof(player_on_map));
    player_on_map[1] = 1;
    state.facility_count = 1;
    state.facility_owner[0] = 0;
    state.facility_type[0] = 1; /* facility type 1 is accepted by 0x41FACC. */
    state.facility_price_status[0] = 0;
    state.facility_level[0] = 2;
    state.route_object_raw[1] = 0xfa1;
    {
        int16_t route_x[2] = {0, 100};
        int16_t route_y[2] = {0, 0};
        uint16_t route_next[2][4] = {{0}};
        assert(rich4_tool_set_map_geometry(
            &state, 2, route_x, route_y, route_next, 0, NULL, NULL,
            0, NULL, NULL));
    }
    decision = select_context_card(&state,
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), player_on_map, 13);
    assert(decision.command.target.kind == GAME_DOMAIN_PLAYER &&
           decision.command.target.id == 1);

    /* The direct facility branch reads the record's +0x24 level-0 charge,
     * not facility_price_per_level. */
    setup(&state, GAME_AI_SCHEMER);
    state.players[0].card_inventory[0] = 14; /* 停留 */
    state.players[0].cash = 11000;
    state.facility_count = 1;
    state.facility_owner[0] = 0;
    state.facility_type[0] = 1;
    state.facility_level[0] = 1;
    state.facility_price_per_level[0] = 20000;
    state.facility_charge_by_level[0][0] = 100;
    current_property = rich4_game_domain_ref(GAME_DOMAIN_FACILITY, 0);
    decision = select_context_card(&state, current_property, player_on_map, 13);
    assert(decision.command.target.kind == GAME_DOMAIN_PLAYER &&
           decision.command.target.id == 0);

    /* A company can be present only in the global typed table.  A player
     * standing on its coordinate is still a valid stop-card target when the
     * route node does not repeat the 0x1771 raw object ID. */
    setup(&state, GAME_AI_SCHEMER);
    state.players[0].card_inventory[0] = 14; /* 停留 */
    state.players[1].node = 1;
    memset(player_on_map, 0, sizeof(player_on_map));
    player_on_map[1] = 1;
    state.company_count = 1;
    state.company_owner[0] = 0;
    state.tool_company_geometry_available = 1;
    state.tool_company_x[0] = 100;
    state.tool_company_y[0] = 0;
    {
        int16_t route_x[2] = {0, 100};
        int16_t route_y[2] = {0, 0};
        uint16_t route_next[2][4] = {{0}};
        assert(rich4_tool_set_map_geometry(
            &state, 2, route_x, route_y, route_next, 0, NULL, NULL,
            0, NULL, NULL));
    }
    decision = select_context_card(&state,
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), player_on_map, 13);
    assert(decision.command.target.kind == GAME_DOMAIN_PLAYER &&
           decision.command.target.id == 1);

    /* The same original scan also accepts an ordinary player standing on an
     * auxiliary/company object owned by the AI.  0x1771 is the first typed
     * special-table-1 route object and maps to company index 0. */
    setup(&state, GAME_AI_SCHEMER);
    state.players[0].card_inventory[0] = 14; /* 停留 */
    state.players[1].node = 1;
    memset(player_on_map, 0, sizeof(player_on_map));
    player_on_map[1] = 1;
    state.company_count = 1;
    state.company_owner[0] = 0;
    state.route_object_raw[1] = 0x1771;
    {
        int16_t route_x[2] = {0, 100};
        int16_t route_y[2] = {0, 0};
        uint16_t route_next[2][4] = {{0}};
        assert(rich4_tool_set_map_geometry(
            &state, 2, route_x, route_y, route_next, 0, NULL, NULL,
            0, NULL, NULL));
    }
    decision = select_context_card(&state,
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), player_on_map, 13);
    assert(decision.command.target.kind == GAME_DOMAIN_PLAYER &&
           decision.command.target.id == 1);

    /* 0x41FACC's visible-player masks use only the ordinary low-nibble
     * players. A special actor at the facility node is not a card target. */
    setup(&state, GAME_AI_SCHEMER);
    state.players[0].card_inventory[0] = 14; /* 停留 */
    state.special_player_f10[0] = 0;
    state.special_player_node[0] = 1;
    state.facility_count = 1;
    state.facility_owner[0] = 0;
    state.facility_type[0] = 1;
    state.facility_level[0] = 2;
    state.route_object_raw[1] = 0xfa1;
    {
        int16_t route_x[2] = {0, 100};
        int16_t route_y[2] = {0, 0};
        uint16_t route_next[2][4] = {{0}};
        assert(rich4_tool_set_map_geometry(
            &state, 2, route_x, route_y, route_next, 0, NULL, NULL,
            0, NULL, NULL));
    }
    memset(player_on_map, 0, sizeof(player_on_map));
    decision = select_context_card_candidate(
        &state, rich4_game_domain_ref(GAME_DOMAIN_NONE, -1),
        player_on_map, 13);
    assert(!decision.has_command &&
           has_result(&decision, GAME_AI_CANDIDATE_RULE_REJECTED));

    /* 0x41F037: angel selects a projected ordinary-land group only when at
     * least three members remain below level five. */
    setup(&state, GAME_AI_SCHEMER);
    state.players[0].card_inventory[0] = 9; /* 天使 */
    state.land_count = 3;
    state.land_group[0] = state.land_group[1] = state.land_group[2] = 6;
    state.land_owner[0] = 0;
    state.land_type[0] = state.land_type[1] = state.land_type[2] = 0;
    state.land_level[0] = 1;
    state.land_level[1] = 2;
    state.land_level[2] = 4;
    state.route_object_raw[1] = 0x7d1;
    state.route_object_raw[2] = 0x7d2;
    state.route_object_raw[3] = 0x7d3;
    {
        int16_t route_x[4] = {0, 0, 100, 200};
        int16_t route_y[4] = {0, 0, 0, 0};
        uint16_t route_next[4][4] = {{0}};
        assert(rich4_tool_set_map_geometry(
            &state, 4, route_x, route_y, route_next, 0, NULL, NULL,
            0, NULL, NULL));
    }
    decision = select_context_card(&state,
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), player_on_map, 8);
    assert(decision.command.target.kind == GAME_DOMAIN_LAND &&
           decision.command.target.id == 0);

    /* The same angel group is visible through the 29x29 tile table even when
     * none of its records is present in the forward route projection. */
    setup(&state, GAME_AI_SCHEMER);
    state.players[0].card_inventory[0] = 9;
    state.players[0].node = 1;
    state.land_count = 3;
    state.land_group[0] = state.land_group[1] = state.land_group[2] = 6;
    state.land_owner[0] = state.land_owner[1] = state.land_owner[2] = 0;
    state.land_type[0] = state.land_type[1] = state.land_type[2] = 0;
    state.land_level[0] = 1;
    state.land_level[1] = 2;
    state.land_level[2] = 4;
    {
        int16_t route_x[4] = {0, 320, 352, 384};
        int16_t route_y[4] = {0, 320, 320, 320};
        int16_t land_x[3] = {320, 352, 384};
        int16_t land_y[3] = {320, 320, 320};
        uint16_t route_next[4][4] = {{0}};
        assert(rich4_tool_set_map_geometry(
            &state, 4, route_x, route_y, route_next, 3, land_x, land_y,
            0, NULL, NULL));
    }
    memset(player_on_map, 0, sizeof(player_on_map));
    decision = select_context_card(&state,
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), player_on_map, 8);
    assert(decision.command.target.kind == GAME_DOMAIN_LAND &&
           decision.command.target.id == 0);

    /* 0x41F1B3: demon prefers the relation player's two built properties
     * totaling seven levels while the actor owns at most one level in the
     * same ordinary-land group. */
    setup(&state, GAME_AI_SCHEMER);
    state.players[0].card_inventory[0] = 10; /* 恶魔 */
    state.players[0].relation_balance[1] = 10;
    state.land_count = 3;
    state.land_group[0] = state.land_group[1] = state.land_group[2] = 7;
    state.land_owner[0] = state.land_owner[1] = 1;
    state.land_owner[2] = 0;
    state.land_type[0] = state.land_type[1] = state.land_type[2] = 0;
    state.land_level[0] = 4;
    state.land_level[1] = 3;
    state.land_level[2] = 1;
    state.route_object_raw[1] = 0x7d1;
    state.route_object_raw[2] = 0x7d2;
    state.route_object_raw[3] = 0x7d3;
    {
        int16_t route_x[4] = {0, 0, 100, 200};
        int16_t route_y[4] = {0, 0, 0, 0};
        uint16_t route_next[4][4] = {{0}};
        assert(rich4_tool_set_map_geometry(
            &state, 4, route_x, route_y, route_next, 0, NULL, NULL,
            0, NULL, NULL));
    }
    decision = select_context_card(&state,
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), player_on_map, 9);
    assert(decision.command.target.kind == GAME_DOMAIN_LAND &&
           decision.command.target.id == 0);

    /* Demon also consumes the visible ordinary-land table, not only route
     * nodes.  The relation player owns the two visible built records. */
    setup(&state, GAME_AI_SCHEMER);
    state.players[0].card_inventory[0] = 10;
    state.players[0].relation_balance[1] = 10;
    state.players[0].node = 1;
    state.land_count = 3;
    state.land_group[0] = state.land_group[1] = state.land_group[2] = 7;
    state.land_owner[0] = state.land_owner[1] = 1;
    state.land_owner[2] = 0;
    state.land_type[0] = state.land_type[1] = state.land_type[2] = 0;
    state.land_level[0] = 4;
    state.land_level[1] = 3;
    state.land_level[2] = 1;
    {
        int16_t route_x[4] = {0, 320, 352, 384};
        int16_t route_y[4] = {0, 320, 320, 320};
        int16_t land_x[3] = {320, 352, 384};
        int16_t land_y[3] = {320, 320, 320};
        uint16_t route_next[4][4] = {{0}};
        assert(rich4_tool_set_map_geometry(
            &state, 4, route_x, route_y, route_next, 3, land_x, land_y,
            0, NULL, NULL));
    }
    memset(player_on_map, 0, sizeof(player_on_map));
    decision = select_context_card(&state,
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), player_on_map, 9);
    assert(decision.command.target.kind == GAME_DOMAIN_LAND &&
           decision.command.target.id == 0);

    /* 0x420055: red card picks the greatest held cost basis, but skips a
     * suspended or already limit-up stock. */
    setup(&state, GAME_AI_SCHEMER);
    state.players[0].card_inventory[0] = 24;
    state.player_stock_amount[0][0] = 20;
    state.player_stock_average_cost[0][0] = 10.0f;
    state.stocks[0].f16 = 10.0f;
    state.stocks[0].f20 = 11.0f;
    state.player_stock_amount[0][1] = 5;
    state.player_stock_average_cost[0][1] = 10.0f;
    state.stocks[1].f16 = 10.0f;
    state.stocks[1].f20 = 10.0f;
    decision = select_context_card(&state,
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), player_on_map, 23);
    assert(decision.command.target.kind == GAME_DOMAIN_STOCK &&
           decision.command.target.id == 1);

    /* 0x4200EA: use the relation-preferred player's best eligible stock when
     * available; otherwise fall back to the best stock held by the player
     * owning the most auxiliary companies.  Company stocks are required only
     * for the first scan, and an ordinary falling quote remains eligible. */
    setup(&state, GAME_AI_SCHEMER);
    state.player_count = 3;
    state.players[2].play_state = 1;
    state.players[0].card_inventory[0] = 25;
    state.company_count = 1;
    state.company_owner[0] = 2;
    state.company_stock_index[0] = 0;
    state.stocks[0].f4 = 1;
    state.stocks[0].f16 = 10.0f;
    state.stocks[0].f20 = 10.0f;
    state.player_stock_amount[2][0] = 10;
    state.player_stock_average_cost[2][0] = 10.0f;
    decision = select_context_card(&state,
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), player_on_map, 24);
    assert(decision.command.target.kind == GAME_DOMAIN_STOCK &&
           decision.command.target.id == 0);

    state.players[0].relation_balance[1] = 10;
    state.player_stock_amount[1][1] = 10;
    state.player_stock_average_cost[1][1] = 20.0f;
    state.stocks[1].f16 = 10.0f;
    state.stocks[1].f20 = 10.0f;
    decision = select_context_card(&state,
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), player_on_map, 24);
    assert(decision.command.target.kind == GAME_DOMAIN_STOCK &&
           decision.command.target.id == 1);

    /* 0x41E9E2: purchase only the built property under the actor's feet.
     * Ordinary land requires the relation-preferred owner at level >= 2; the
     * handler uses a strict cost < cash check. */
    setup(&state, GAME_AI_SCHEMER);
    state.players[0].card_inventory[0] = 3;
    state.players[0].cash = 200;
    state.players[0].relation_balance[1] = 10;
    state.land_count = 1;
    state.land_owner[0] = 1;
    state.land_level[0] = 2;
    state.land_price[0] = 100;
    state.land_price_per_level[0] = 10;
    current_property = rich4_game_domain_ref(GAME_DOMAIN_LAND, 0);
    decision = select_context_card(&state, current_property, player_on_map, 2);
    assert(decision.command.target.kind == GAME_DOMAIN_LAND &&
           decision.command.target.id == 0);

    /* Built facilities use the parallel base + level * increment formula and
     * do not require the owner to be the relation-preferred player. */
    setup(&state, GAME_AI_SCHEMER);
    state.players[0].card_inventory[0] = 3;
    state.players[0].cash = 200;
    state.players[0].relation_balance[1] = 10;
    state.facility_count = 1;
    state.facility_owner[0] = 1;
    state.facility_level[0] = 1;
    state.facility_price[0] = 100;
    state.facility_price_per_level[0] = 10;
    current_property = rich4_game_domain_ref(GAME_DOMAIN_FACILITY, 0);
    decision = select_context_card(&state, current_property, player_on_map, 2);
    assert(decision.command.target.kind == GAME_DOMAIN_FACILITY &&
           decision.command.target.id == 0);

    /* 0x41EF26 accepts another owner's level >=3 without a relation target. */
    setup(&state, GAME_AI_SCHEMER);
    state.players[0].card_inventory[0] = 8;
    state.land_count = 1;
    state.land_owner[0] = 1;
    state.land_level[0] = 3;
    current_property = rich4_game_domain_ref(GAME_DOMAIN_LAND, 0);
    decision = select_context_card(&state, current_property, player_on_map, 7);
    assert(decision.command.target.kind == GAME_DOMAIN_LAND &&
           decision.command.target.id == 0);

    setup(&state, GAME_AI_SCHEMER);
    state.players[0].card_inventory[0] = 8;
    state.players[0].relation_balance[1] = 10;
    state.land_count = 1;
    state.land_owner[0] = 1;
    state.land_level[0] = 3;
    current_property = rich4_game_domain_ref(GAME_DOMAIN_LAND, 0);
    decision = select_context_card(&state, current_property, player_on_map, 7);
    assert(decision.command.target.kind == GAME_DOMAIN_LAND &&
           decision.command.target.id == 0);

    /* 0x42040E: price increase accepts a visible land group when the actor
     * owns at least two thirds of it, has seven built levels, and the other
     * owners have at most three. */
    setup(&state, GAME_AI_SCHEMER);
    memset(player_on_map, 0, sizeof(player_on_map));
    state.players[0].card_inventory[0] = 27;
    state.land_count = 3;
    state.land_group[0] = state.land_group[1] = state.land_group[2] = 4;
    state.land_owner[0] = state.land_owner[1] = 0;
    state.land_owner[2] = 1;
    state.land_level[0] = 4;
    state.land_level[1] = 3;
    state.land_level[2] = 1;
    {
        int16_t route_x[4] = {0, 0, 100, 200};
        int16_t route_y[4] = {0, 0, 0, 0};
        uint16_t route_next[4][4] = {{0}};
        state.route_object_raw[1] = 0x7d1;
        state.route_object_raw[2] = 0x7d2;
        state.route_object_raw[3] = 0x7d3;
        assert(rich4_tool_set_map_geometry(
            &state, 4, route_x, route_y, route_next, 0, NULL, NULL,
            0, NULL, NULL));
    }
    decision = select_context_card(
        &state, rich4_game_domain_ref(GAME_DOMAIN_NONE, -1),
        player_on_map, 26);
    assert(decision.command.target.kind == GAME_DOMAIN_LAND &&
           decision.command.target.id == 0);

    /* 0x40A0B1/0x40A45C: when property coordinates are available, the
     * visible-object table is a 29x29 tile window, not the forward route
     * list.  None of these records is on a route node, but all three are in
     * the active player's viewport and the first group member is selected. */
    setup(&state, GAME_AI_SCHEMER);
    memset(player_on_map, 0, sizeof(player_on_map));
    state.players[0].card_inventory[0] = 27;
    state.players[0].node = 1;
    state.land_count = 3;
    state.land_group[0] = state.land_group[1] = state.land_group[2] = 4;
    state.land_owner[0] = state.land_owner[1] = 0;
    state.land_owner[2] = 1;
    state.land_level[0] = 4;
    state.land_level[1] = 3;
    state.land_level[2] = 1;
    {
        int16_t route_x[3] = {0, 320, 352};
        int16_t route_y[3] = {0, 320, 320};
        int16_t land_x[3] = {320, 352, 384};
        int16_t land_y[3] = {320, 320, 320};
        uint16_t route_next[3][4] = {{0}};
        assert(rich4_tool_set_map_geometry(
            &state, 3, route_x, route_y, route_next, 3, land_x, land_y,
            0, NULL, NULL));
    }
    decision = select_context_card(
        &state, rich4_game_domain_ref(GAME_DOMAIN_NONE, -1),
        player_on_map, 26);
    assert(decision.command.target.kind == GAME_DOMAIN_LAND &&
           decision.command.target.id == 0);

    /* The facility fallback reads the original +0x18 type byte, not the
     * price-status byte.  Type 1 remains selectable even with status zero. */
    setup(&state, GAME_AI_SCHEMER);
    memset(player_on_map, 0, sizeof(player_on_map));
    state.players[0].card_inventory[0] = 27;
    state.facility_count = 1;
    state.facility_owner[0] = 0;
    state.facility_type[0] = 1;
    state.facility_price_status[0] = 0;
    state.facility_level[0] = 3;
    {
        int16_t route_x[3] = {0, 0, 100};
        int16_t route_y[3] = {0, 0, 0};
        uint16_t route_next[3][4] = {{0}};
        state.route_object_raw[1] = 0xfa1;
        assert(rich4_tool_set_map_geometry(
            &state, 3, route_x, route_y, route_next, 0, NULL, NULL,
            0, NULL, NULL));
    }
    decision = select_context_card(
        &state, rich4_game_domain_ref(GAME_DOMAIN_NONE, -1),
        player_on_map, 26);
    assert(decision.command.target.kind == GAME_DOMAIN_FACILITY &&
           decision.command.target.id == 0);

    /* 0x42062B: the six-node route branch selects a land group carrying at
     * least seven built levels owned by other players. */
    setup(&state, GAME_AI_SCHEMER);
    memset(player_on_map, 0, sizeof(player_on_map));
    state.players[0].card_inventory[0] = 28;
    state.players[0].node = 1;
    state.land_count = 2;
    state.land_group[0] = state.land_group[1] = 5;
    state.land_owner[0] = state.land_owner[1] = 1;
    state.land_level[0] = 4;
    state.land_level[1] = 3;
    {
        int16_t route_x[3] = {0, 0, 100};
        int16_t route_y[3] = {0, 0, 0};
        uint16_t route_next[3][4] = {{0}};
        route_next[1][0] = 2;
        state.route_object_raw[2] = 0x7d1;
        assert(rich4_tool_set_map_geometry(
            &state, 3, route_x, route_y, route_next, 0, NULL, NULL,
            0, NULL, NULL));
    }
    decision = select_context_card(
        &state, rich4_game_domain_ref(GAME_DOMAIN_NONE, -1),
        player_on_map, 27);
    assert(decision.command.target.kind == GAME_DOMAIN_LAND &&
           decision.command.target.id == 0);

    /* The facility branch likewise ignores price status, but the original
     * requires the facility to belong to the relation-preferred player. */
    setup(&state, GAME_AI_SCHEMER);
    memset(player_on_map, 0, sizeof(player_on_map));
    state.players[0].card_inventory[0] = 28;
    state.players[0].relation_balance[1] = 10;
    state.players[0].node = 1;
    state.facility_count = 1;
    state.facility_owner[0] = 1;
    state.facility_type[0] = 1;
    state.facility_price_status[0] = 0;
    state.facility_level[0] = 3;
    {
        int16_t route_x[3] = {0, 0, 100};
        int16_t route_y[3] = {0, 0, 0};
        uint16_t route_next[3][4] = {{0}};
        route_next[1][0] = 2;
        state.route_object_raw[2] = 0xfa1;
        assert(rich4_tool_set_map_geometry(
            &state, 3, route_x, route_y, route_next, 0, NULL, NULL,
            0, NULL, NULL));
    }
    decision = select_context_card(
        &state, rich4_game_domain_ref(GAME_DOMAIN_NONE, -1),
        player_on_map, 27);
    assert(decision.command.target.kind == GAME_DOMAIN_FACILITY &&
           decision.command.target.id == 0);

    /* A self-owned facility is not the card-28 facility branch. */
    setup(&state, GAME_AI_SCHEMER);
    memset(player_on_map, 0, sizeof(player_on_map));
    state.players[0].card_inventory[0] = 28;
    state.players[0].relation_balance[1] = 10;
    state.players[0].node = 1;
    state.facility_count = 1;
    state.facility_owner[0] = 0;
    state.facility_type[0] = 1;
    state.facility_level[0] = 3;
    {
        int16_t route_x[3] = {0, 0, 100};
        int16_t route_y[3] = {0, 0, 0};
        uint16_t route_next[3][4] = {{0}};
        route_next[1][0] = 2;
        state.route_object_raw[2] = 0xfa1;
        assert(rich4_tool_set_map_geometry(
            &state, 3, route_x, route_y, route_next, 0, NULL, NULL,
            0, NULL, NULL));
    }
    decision = select_context_card_candidate(
        &state, rich4_game_domain_ref(GAME_DOMAIN_NONE, -1),
        player_on_map, 27);
    assert(!decision.has_command);
    {
        int found = 0;
        int i;
        for (i = 0; i < decision.candidate_count; i++)
            if (decision.candidates[i].subject.kind == GAME_DOMAIN_CARD &&
                decision.candidates[i].subject.id == 27 &&
                decision.candidates[i].result ==
                    GAME_AI_CANDIDATE_RULE_REJECTED)
                found = 1;
        assert(found);
    }

    /* A forward fork is resolved by the normal route RNG; a seed that chooses
     * the facility branch must still reach the same relation-owned target. */
    setup(&state, GAME_AI_SCHEMER);
    memset(player_on_map, 0, sizeof(player_on_map));
    state.players[0].card_inventory[0] = 28;
    state.players[0].relation_balance[1] = 10;
    state.players[0].node = 1;
    state.facility_count = 1;
    state.facility_owner[0] = 1;
    state.facility_type[0] = 1;
    state.facility_level[0] = 3;
    {
        int16_t route_x[4] = {0, 0, 100, 100};
        int16_t route_y[4] = {0, 0, 0, 100};
        uint16_t route_next[4][4] = {{0}};
        route_next[1][0] = 2;
        route_next[1][1] = 3;
        state.route_object_raw[2] = 0;
        state.route_object_raw[3] = 0xfa1;
        assert(rich4_tool_set_map_geometry(
            &state, 4, route_x, route_y, route_next, 0, NULL, NULL,
            0, NULL, NULL));
    }
    decision = select_context_card(&state,
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), player_on_map, 27);
    assert(decision.command.target.kind == GAME_DOMAIN_FACILITY &&
           decision.command.target.id == 0);

    /* 0x41FE4E: winter sleep has no target-selection logic; the AI helper
     * accepts it only when rand()%4 is zero, then the normal card inspector
     * verifies that another active role exists. */
    setup(&state, GAME_AI_SCHEMER);
    memset(player_on_map, 0, sizeof(player_on_map));
    state.players[0].card_inventory[0] = 15;
    current_property = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
    for (seed = 1; seed < 10000 &&
                     (!saw_winter_sleep_selected ||
                      !saw_winter_sleep_random_gate); seed++) {
        assert(rich4_ai_select_command_with_map(
            &state, current_property, player_on_map, 0,
            GAME_AI_PHASE_CARD_OR_TOOL, seed, &decision));
        for (i = 0; i < decision.candidate_count; i++) {
            if (decision.candidates[i].subject.kind != GAME_DOMAIN_CARD ||
                decision.candidates[i].subject.id != 14) continue;
            if (decision.candidates[i].result == GAME_AI_CANDIDATE_SELECTED) {
                saw_winter_sleep_selected = 1;
                assert(decision.has_command &&
                       decision.command.type == GAME_COMMAND_USE_CARD &&
                       decision.command.subject.id == 14 &&
                       decision.command.target.kind == GAME_DOMAIN_NONE);
            }
            if (decision.candidates[i].result ==
                GAME_AI_CANDIDATE_RANDOM_GATE)
                saw_winter_sleep_random_gate = 1;
        }
    }
    assert(saw_winter_sleep_selected && saw_winter_sleep_random_gate);

    /* 0x4202D2: tax prefers the visible relation target above 30,000 *
     * price_index, then falls back to the first visible target above 50,000. */
    setup(&state, GAME_AI_SCHEMER);
    memset(player_on_map, 0, sizeof(player_on_map));
    state.players[0].card_inventory[0] = 26;
    state.players[1].cash = 40000;
    state.players[0].relation_balance[1] = 10;
    player_on_map[1] = 1;
    decision = select_context_card(&state,
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), player_on_map, 25);
    assert(decision.command.target.kind == GAME_DOMAIN_PLAYER &&
           decision.command.target.id == 1);

    setup(&state, GAME_AI_SCHEMER);
    state.player_count = 3;
    memset(player_on_map, 0, sizeof(player_on_map));
    state.players[0].card_inventory[0] = 26;
    state.players[1].cash = 60000;
    state.players[2].cash = 40000;
    player_on_map[1] = 1;
    player_on_map[2] = 1;
    decision = select_context_card(&state,
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), player_on_map, 25);
    assert(decision.command.target.kind == GAME_DOMAIN_PLAYER &&
           decision.command.target.id == 1);

    /* 0x4207CC: count all active players first, then require the strict
     * winner to be visible, non-preferred and not already allied. */
    setup(&state, GAME_AI_SCHEMER);
    state.player_count = 3;
    state.players[0].linked_player = -1;
    state.players[1].linked_player = -1;
    state.players[2].linked_player = -1;
    state.players[2].play_state = 1;
    memset(player_on_map, 0, sizeof(player_on_map));
    state.players[0].card_inventory[0] = 29;
    state.players[0].relation_balance[1] = 10;
    player_on_map[1] = 1;
    player_on_map[2] = 1;
    state.land_count = 3;
    state.land_owner[0] = 1;
    state.land_owner[1] = 2;
    state.land_owner[2] = 2;
    state.facility_count = 1;
    state.facility_owner[0] = 2;
    decision = select_context_card(&state,
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), player_on_map, 28);
    assert(decision.command.target.kind == GAME_DOMAIN_PLAYER &&
           decision.command.target.id == 2);

    /* If the strict property-count winner is the relation-preferred player,
     * the original function rejects the card instead of selecting runner-up. */
    state.land_count = 4;
    state.land_owner[0] = state.land_owner[1] = state.land_owner[2] = 1;
    state.land_owner[3] = 2;
    state.facility_count = 0;
    decision = select_context_card_candidate(&state,
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), player_on_map, 28);
    assert(!decision.has_command &&
           has_result(&decision, GAME_AI_CANDIDATE_RULE_REJECTED));
}

static GameAiDecision projected_tool_decision(
    const GameState *state, const GameAiGlobalProjection *projection,
    uint32_t seed)
{
    GameAiDecision d;
    assert(rich4_ai_select_command_with_map_context_and_projection(
        state, rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), NULL, NULL,
        projection, 0, GAME_AI_PHASE_CARD_OR_TOOL, seed, &d));
    return d;
}

static void ida_tool_selection_regressions(void)
{
    GameState state;
    GameAiDecision d;
    GameAiGlobalProjection projection;
    GameAiGlobalObjectProjection objects[2];
    Rich4Rng expected;
    uint32_t seed;
    int i;
    int16_t route_x[4] = {0, 1024, 128, 256};
    int16_t route_y[4] = {0};
    uint16_t route_next[4][4] = {{0}};
    uint8_t placeable[4] = {0, 1, 1, 1};
    int16_t facility_x[1] = {128};
    int16_t facility_y[1] = {0};
    int16_t land_x[6] = {128, 160, 192, 224, 256, 288};
    int16_t land_y[6] = {0};

    /* 0x447D97 wraps a one-kind list for four random eligibility attempts. */
    setup(&state, GAME_AI_SCHEMER);
    state.players[0].ai_action_flags = 2;
    state.players[0].tool_inventory[4] = 9;
    for (seed = 1; seed < 10000; seed++) {
        int eligible = 1;
        rich4_rng_state_seed(&expected, seed);
        if ((rich4_rng_original_state_next(&expected) & 1) != 0) continue;
        for (i = 0; i < 4; i++)
            if (rich4_rng_original_state_range(&expected, 4) == 0) eligible = 0;
        if (eligible) break;
    }
    assert(seed < 10000);
    d = projected_tool_decision(&state, NULL, seed);
    assert(!d.has_command && d.candidate_count == 4 &&
           d.rng_after == expected.state);
    for (i = 0; i < 4; i++)
        assert(d.candidates[i].subject.id == 4 &&
               d.candidates[i].result == GAME_AI_CANDIDATE_RANDOM_GATE);

    /* 0x421CB6 consumes only the branch draw even with branching roads.
     * Fixed facility charge zero satisfies its strict cash comparison. */
    setup(&state, GAME_AI_SCHEMER);
    state.players[0].ai_action_flags = 2;
    state.players[0].tool_inventory[10] = 1;
    state.players[0].node = 1;
    state.players[0].cash = 10001;
    state.facility_count = 1;
    state.facility_owner[0] = -1;
    state.facility_type[0] = 1;
    state.facility_level[0] = 3;
    route_next[1][0] = 2;
    route_next[1][1] = 3;
    assert(rich4_tool_set_route_targets(&state, 4, placeable, sizeof(placeable)));
    assert(rich4_tool_set_map_geometry(&state, 4, route_x, route_y,
        route_next, 0, NULL, NULL, 1, facility_x, facility_y));
    state.route_object_raw[2] = 0xfa1;
    memset(&projection, 0, sizeof(projection));
    projection.center_world_x = 192;
    d = projected_tool_decision(&state, &projection, 1);
    rich4_rng_state_seed(&expected, 1);
    assert((rich4_rng_original_state_next(&expected) & 1) == 0);
    assert(d.has_command && d.command.subject.id == 10 &&
           d.command.target2.id == 2 && d.rng_after == expected.state);

    /* 0x421717 uses the preferred player once, independent of traffic;
     * absent preferred-player pixels reject that fixed target. */
    state.players[0].tool_inventory[10] = 0;
    state.players[0].tool_inventory[6] = 1;
    state.players[0].traffic_method = 3;
    state.players[0].relation_balance[1] = 5;
    state.players[1].node = 2;
    state.players[1].days_in_prison = 1;
    projection.object_table_available = 1;
    projection.objects = objects;
    projection.object_count = 1;
    objects[0].raw_id = 0x8002;
    objects[0].screen_x = objects[0].screen_y = 220;
    projection.positioned_player_mask = 3;
    projection.player_world_x[0] = 1024;
    projection.player_world_x[1] = 128;
    d = projected_tool_decision(&state, &projection, 1);
    assert(d.has_command && d.command.subject.id == 6 &&
           d.command.target.id == 1 && d.rng_after == expected.state);
    state.player_count = 3;
    state.players[2].play_state = 1;
    state.players[2].node = 3;
    objects[0].raw_id = 0x8004;
    d = projected_tool_decision(&state, &projection, 1);
    assert(!d.has_command && d.rng_after == expected.state);
    /* Random fallback checks all four packed status bytes. */
    state.player_count = 2;
    state.players[0].relation_balance[1] = 0;
    objects[0].raw_id = 0x8002;
    d = projected_tool_decision(&state, &projection, 1);
    assert(!d.has_command && d.rng_after == expected.state);

    /* 0x4213C5 reads the four-player prison flags before random land choice. */
    state.players[0].tool_inventory[6] = 0;
    state.players[0].tool_inventory[2] = 1;
    state.players[0].previous_node = 2;
    state.players[1].confinement_actor_type = GAME_CONFINEMENT_PRISON;
    state.prison_node = 2;
    route_next[2][0] = 3;
    assert(rich4_tool_set_map_geometry(&state, 4, route_x, route_y,
        route_next, 0, NULL, NULL, 0, NULL, NULL));
    d = projected_tool_decision(&state, &projection, 1);
    assert(d.has_command && d.command.subject.id == 2 &&
           d.command.target.id == 2);

    /* 0x421E62 counts surviving owned-property pixels and their actual
     * levels. The five external assets include four level-zero records. */
    setup(&state, GAME_AI_SCHEMER);
    state.players[0].ai_action_flags = 2;
    state.players[0].tool_inventory[12] = 1;
    state.players[0].node = 1;
    state.land_count = 6;
    for (i = 0; i < 5; i++) state.land_owner[i] = 1;
    state.land_owner[5] = 0;
    state.land_level[0] = 1;
    state.land_level[5] = 1;
    assert(rich4_tool_set_map_geometry(&state, 4, route_x, route_y,
        route_next, 6, land_x, land_y, 0, NULL, NULL));
    memset(&projection, 0, sizeof(projection));
    projection.positioned_player_mask = 1;
    projection.player_world_x[0] = 1024;
    d = projected_tool_decision(&state, &projection, 1);
    assert(!d.has_command); /* Level ratio 1/1 fails. */
    state.land_level[5] = 0;
    d = projected_tool_decision(&state, &projection, 1);
    assert(d.has_command && d.command.target.kind == GAME_DOMAIN_LAND &&
           d.command.target.id == 0); /* Count 1/5, level 0/1. */
    /* The later self land overwrites the sole built external land's pixel.
     * The remaining external levels total zero, so 0/0 rejects the target. */
    state.tool_land_x[5] = state.tool_land_x[0];
    d = projected_tool_decision(&state, &projection, 1);
    assert(!d.has_command);
    state.tool_land_x[5] = 288;
    /* Current actor alone enters 0x40A0B1's actor pass; its four-byte
     * status gate permits the scan when a confinement byte is set. */
    projection.player_world_x[0] = 96;
    d = projected_tool_decision(&state, &projection, 1);
    assert(!d.has_command);
    state.players[0].days_in_hospital = 1;
    d = projected_tool_decision(&state, &projection, 1);
    assert(d.has_command);
}

static GameAiDecision alignment_card(const GameState *s, int card,
    GameDomainRef current, const GameAiGlobalProjection *projection,
    const GameAiMapTargetContext *context, const GameAiSelectionMemory *memory)
{
    GameAiDecision d;
    uint32_t seed;
    for (seed = 1; seed < 10000; seed++) {
        int i;
        assert(rich4_ai_select_command_with_selection_memory(
            s, current, NULL, context, projection, memory, 0,
            GAME_AI_PHASE_CARD_OR_TOOL, seed, &d));
        for (i = 0; i < d.candidate_count; i++)
            if (d.candidates[i].subject.kind == GAME_DOMAIN_CARD &&
                d.candidates[i].subject.id == card) return d;
    }
    assert(!"alignment card candidate missing");
    return d;
}

static int alignment_stock_score(const GameState *s, GameAiPhase phase,
                                 int stock, int distress)
{
    GameAiSelectionMemory memory = {0, 0};
    GameAiDecision d;
    uint32_t seed;
    memory.stock_distress = distress;
    for (seed = 1; seed < 10000; seed++) {
        int i;
        assert(rich4_ai_select_command_with_selection_memory(
            s, rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), NULL, NULL,
            NULL, &memory, 0, phase, seed, &d));
        for (i = 0; i < d.candidate_count; i++)
            if (d.candidates[i].subject.kind == GAME_DOMAIN_STOCK &&
                d.candidates[i].subject.id == stock) return d.candidates[i].score;
    }
    assert(!"alignment stock candidate missing");
    return -1;
}

static void original_strategy_alignment(void)
{
    GameState s;
    GameAiDecision d;
    GameAiSelectionMemory memory = {0, 0};
    GameAiMapTargetContext context;
    GameAiGlobalProjection projection;
    GameAiGlobalObjectProjection objects[4];
    GameDomainRef none = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
    Rich4Rng rng = {1};
    const int original_rolls[5] = {16838, 5758, 10113, 17515, 31051};
    int i;
    /* 0x456F2D: raw Watcom sequence, independent of host libc. */
    for (i = 0; i < 5; i++)
        assert(rich4_rng_original_state_next(&rng) == original_rolls[i]);

    /* 0x41EF26: all four boundary/owner combinations. */
    setup(&s, GAME_AI_SCHEMER);
    s.players[0].card_inventory[0] = 8;
    s.land_count = 1;
    s.land_owner[0] = 1;
    for (i = 0; i <= 3; i++) {
        s.land_level[0] = i;
        d = alignment_card(&s, 7,
            rich4_game_domain_ref(GAME_DOMAIN_LAND, 0), NULL, NULL, NULL);
        assert(d.has_command == (i >= 3));
    }
    s.players[0].relation_balance[1] = 1;
    for (i = 0; i <= 3; i++) {
        s.land_level[0] = i;
        d = alignment_card(&s, 7,
            rich4_game_domain_ref(GAME_DOMAIN_LAND, 0), NULL, NULL, NULL);
        assert(d.has_command == (i >= 2));
    }

    /* 0x41FFF8 compares dynamic slot, not god type. */
    setup(&s, GAME_AI_SCHEMER);
    s.players[0].card_inventory[0] = 23;
    context.nearest_dynamic_god = rich4_game_domain_ref(GAME_DOMAIN_GOD, 7);
    context.nearest_dynamic_god_type = 1;
    d = alignment_card(&s, 22, none, NULL, &context, NULL);
    assert(!d.has_command && has_result(&d, GAME_AI_CANDIDATE_RULE_REJECTED));
    context.nearest_dynamic_god.id = 11;
    context.nearest_dynamic_god_type = 15;
    d = alignment_card(&s, 22, none, NULL, &context, NULL);
    assert(d.has_command && d.selection_after.card_target_raw == 12);
    s.players[0].god_info = 1;
    s.players[0].god_type = 15;
    d = alignment_card(&s, 22, none, NULL, &context, NULL);
    assert(!d.has_command);

    /* 0x41FC0B preserves a target from a previous turn. */
    setup(&s, GAME_AI_SCHEMER);
    s.players[0].card_inventory[0] = 14;
    s.players[0].cash = 11000;
    s.land_count = 1;
    s.land_owner[0] = 0;
    s.land_level[0] = 2;
    memory.card_target_raw = 0x8002;
    d = alignment_card(&s, 13,
        rich4_game_domain_ref(GAME_DOMAIN_LAND, 0), NULL, NULL, &memory);
    assert(d.has_command && d.command.target.kind == GAME_DOMAIN_PLAYER &&
           d.command.target.id == 1 && d.selection_after.card_target_raw == 0x8002);
    memory.card_target_raw = 0;
    d = alignment_card(&s, 13,
        rich4_game_domain_ref(GAME_DOMAIN_LAND, 0), NULL, NULL, &memory);
    assert(d.has_command && d.command.target.kind == GAME_DOMAIN_NONE);

    memset(&projection, 0, sizeof(projection));
    projection.object_table_available = 1;
    projection.objects = objects;
    projection.object_count = 2;
    objects[0].screen_x = 10; objects[0].screen_y = 10;
    objects[1].screen_x = 20; objects[1].screen_y = 20;
    /* 0x4205F6 inherits ESI=8 from the one-card inventory window. */
    setup(&s, GAME_AI_SCHEMER);
    s.players[0].card_inventory[0] = 27;
    s.facility_count = 2;
    s.facility_owner[0] = s.facility_owner[1] = 0;
    s.facility_type[0] = s.facility_type[1] = 1;
    s.facility_level[0] = 3;
    s.facility_level[1] = 5;
    objects[0].raw_id = 0xfa1;
    objects[1].raw_id = 0xfa2;
    d = alignment_card(&s, 26, none, &projection, NULL, NULL);
    assert(d.has_command && d.command.target.id == 0);

    /* 0x41F400 keeps the first equal-level/equal-price screen item. */
    setup(&s, GAME_AI_SCHEMER);
    s.players[0].card_inventory[0] = 11;
    s.players[0].relation_balance[1] = 1;
    s.land_count = 2;
    s.land_owner[0] = s.land_owner[1] = 1;
    s.land_level[0] = s.land_level[1] = 4;
    s.land_price[0] = s.land_price[1] = 100;
    objects[0].raw_id = 0x7d2;
    objects[1].raw_id = 0x7d1;
    d = alignment_card(&s, 10, none, &projection, NULL, NULL);
    assert(d.has_command && d.command.target.id == 1);

    /* 0x41F1B3 forbids self built levels when no relation target exists. */
    setup(&s, GAME_AI_SCHEMER);
    s.players[0].card_inventory[0] = 10;
    s.land_count = 4;
    projection.object_count = 4;
    for (i = 0; i < 4; i++) {
        objects[i].raw_id = (uint16_t)(0x7d1 + i);
        objects[i].screen_x = 10 + i * 10;
        objects[i].screen_y = 10 + i * 10;
        s.land_group[i] = 1;
        s.land_owner[i] = i == 0 ? 0 : 1;
        s.land_level[i] = i == 0 ? 1 : 3;
    }
    d = alignment_card(&s, 9, none, &projection, NULL, NULL);
    assert(!d.has_command);
    s.land_level[0] = 0;
    d = alignment_card(&s, 9, none, &projection, NULL, NULL);
    assert(d.has_command);

    /* 0x42BF03 must not treat a loss as a positive business tier. */
    setup(&s, GAME_AI_SCHEMER);
    s.players[0].bank = 100000;
    s.players[0].ai_stock_ratio = 50;
    s.company_count = 1;
    s.stock_company_auxiliary[0] = 0;
    s.stock_company_value[0] = 100000;
    s.stocks[0].f4 = 1;
    s.stocks[0].f10 = 100;
    s.stocks[0].f16 = s.stocks[0].f20 = 11.0f;
    s.company_total_business[0] = (uint32_t)-1;
    s.company_period_business[0] = (uint32_t)-1;
    assert(alignment_stock_score(&s, GAME_AI_PHASE_STOCK_BUY, 0, 0) == 0);
    s.company_period_business[0] = 1;
    assert(alignment_stock_score(&s, GAME_AI_PHASE_STOCK_BUY, 0, 0) == 1);

    /* 0x42C79F uses total/own and company distress +1. */
    setup(&s, GAME_AI_SCHEMER);
    s.players[0].cash = 40000;
    s.calendar_start_year = 2026; s.calendar_start_month = 8;
    s.calendar_start_day = 11;
    s.company_count = 1;
    s.stock_company_auxiliary[0] = 0;
    s.stock_company_value[0] = 100000;
    s.company_owner[0] = 0;
    s.company_period_business[0] = (uint32_t)-10000;
    s.stocks[0].f4 = 1;
    s.stocks[0].f16 = s.stocks[0].f20 = 10.0f;
    s.player_stock_amount[0][0] = 20;
    s.player_stock_amount[1][0] = 100;
    s.player_stock_average_cost[0][0] = 10.0f;
    assert(alignment_stock_score(&s, GAME_AI_PHASE_STOCK_SELL, 0, 0) == 3);
    s.players[0].loan = 50000;
    assert(alignment_stock_score(&s, GAME_AI_PHASE_STOCK_SELL, 0, 1) == 4);

    /* Historical floor uses 8*m > 1.25*c, and limits reject candidates. */
    setup(&s, GAME_AI_SCHEMER);
    s.players[0].cash = 40000;
    s.player_stock_amount[0][0] = 10;
    s.player_stock_average_cost[0][0] = 10.0f;
    s.stocks[0].f16 = s.stocks[0].f20 = 20.0f;
    s.stocks[0].f28 = 2.0f;
    s.stock_history[0][0] = 2.0f;
    assert(alignment_stock_score(&s, GAME_AI_PHASE_STOCK_SELL, 0, 0) == 3);
    s.stock_history[0][0] = 1.0f;
    assert(alignment_stock_score(&s, GAME_AI_PHASE_STOCK_SELL, 0, 0) == 1);
    s.stocks[0].f16 = 30.0f;
    assert(alignment_stock_score(&s, GAME_AI_PHASE_STOCK_SELL, 0, 0) == 0);

    /* Repeat-sale mode survives crossing principal and stops at 1.1*loan. */
    s.stocks[0].f16 = 20.0f;
    s.players[0].loan = 1000;
    s.players[0].cash = 1050;
    memory.stock_distress = 1;
    assert(rich4_ai_select_command_with_selection_memory(
        &s, none, NULL, NULL, NULL, &memory, 0,
        GAME_AI_PHASE_STOCK_SELL, 7, &d));
    assert(d.has_command && d.next_phase == GAME_AI_PHASE_STOCK_SELL &&
           d.rng_after == 7 && d.selection_after.stock_distress);
    s.players[0].cash = 1100;
    assert(rich4_ai_select_command_with_selection_memory(
        &s, none, NULL, NULL, NULL, &memory, 0,
        GAME_AI_PHASE_STOCK_SELL, 7, &d));
    assert(!d.has_command && d.rng_after == 7 &&
           !d.selection_after.stock_distress);
}

static void original_dice_count_strategy(void)
{
    GameState s;
    int16_t x[7] = {0, 10, 20, 30, 40, 50, 60};
    int16_t y[7] = {0};
    uint16_t next[7][4] = {{0}};
    uint32_t after;
    int count, i;
    setup(&s, GAME_AI_SCHEMER);
    s.players[0].node = 1;
    s.players[0].previous_node = 0;
    s.players[0].traffic_method = 2;
    s.players[0].ndices = 3;
    s.land_count = 3;
    for (i = 1; i < 6; i++) next[i][0] = (uint16_t)(i + 1);
    assert(rich4_tool_set_map_geometry(
        &s, 7, x, y, next, 0, NULL, NULL, 0, NULL, NULL));
    s.route_object_raw[2] = 0x7d1;
    s.route_object_raw[3] = 0x7d2;
    s.route_object_raw[4] = 0x7d3;
    s.land_owner[0] = 0; s.land_owner[1] = -1; s.land_owner[2] = 1;
    assert(rich4_ai_select_dice_count(&s, 0, 7, &count, &after));
    assert(count == 1 && after == 7);
    for (i = 0; i < 3; i++) s.land_owner[i] = 1;
    assert(rich4_ai_select_dice_count(&s, 0, 7, &count, &after));
    assert((count == 2 || count == 3) && after != 7);
    s.players[0].attached_timed_bomb_slot = 0;
    s.tool_placed_objects[0].kind = 0x12;
    s.tool_placed_objects[0].attached_player = 1;
    s.tool_placed_objects[0].countdown = 14;
    assert(rich4_ai_select_dice_count(&s, 0, 7, &count, &after));
    assert(count == 1 && after == 7);
    s.tool_placed_objects[0].countdown = 15;
    assert(rich4_ai_select_dice_count(&s, 0, 7, &count, &after));
    assert(count == 3 && after == 7);
    s.players[0].remote_dice_value = 4;
    s.players[0].ndices = 2;
    assert(rich4_ai_select_dice_count(&s, 0, 7, &count, &after));
    assert(count == 2 && after == 7);
}

int main(void)
{
    original_strategy_alignment();
    original_dice_count_strategy();
    ida_tool_selection_regressions();
    personalities_and_cards();
    tools_and_deferred_position();
    projected_ai_context_fallback();
    route_tool_target_window();
    robot_tool_target_policy();
    area_and_transfer_tool_targets();
    projected_global_object_subset();
    renderer_global_table_projection();
    vehicle_tool_ai_gates();
    remote_dice_ai_gates();
    machine_doll_route_gate();
    tortoise_card_ai();
    ai_dynamic_god_target_context();
    original_ai_noop_cards_are_rejected();
    stock_buy_sell_and_closed();
    stock_buy_truncation();
    company_stock_buy_and_loan_gate();
    company_stock_sell();
    independent_stock_sell_thresholds();
    upgrade_move_and_empty();
    shared_purchase_reserve_policy();
    original_card_target_strategies();
    puts("original three-profile AI command selector: ok");
    return 0;
}
