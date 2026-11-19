#include "app/card_use_flow.h"

#include <string.h>

#include "app/game_speech_flow.h"
#include "app/card_defense_flow.h"
#include "app/card_effect_flow.h"
#include "app/route_geometry_flow.h"
#include "app/special_place_notice_flow.h"
#include "core/card.h"
#include "core/stock.h"
#include "core/game_event_payload.h"
#include "data/character_speech.h"

int rich4_card_use_animation_resource(int card, GameDomainRef target)
{
    if (target.kind != GAME_DOMAIN_LAND &&
        target.kind != GAME_DOMAIN_FACILITY) return 0;
    return card == 10 ? 557 : card == 11 ? 529 : 0;
}

static void card_target_position(GameCardUsePresentationState *use,
    const GameSession *session, const MapData *map)
{
    GameDomainRef target = use->command.target;
    int node = -1;
    if (target.id < 0) return;
    if (target.kind == GAME_DOMAIN_LAND && target.id < session->state.land_count) {
        use->victim_player = session->state.land_owner[target.id];
        use->previous_building_type = session->state.land_type[target.id];
        use->previous_building_level = session->state.land_level[target.id];
    } else if (target.kind == GAME_DOMAIN_FACILITY && target.id < session->state.facility_count) {
        use->victim_player = session->state.facility_owner[target.id];
        use->previous_building_type = session->state.facility_type[target.id];
        use->previous_building_level = session->state.facility_level[target.id];
    }
    if (target.kind == GAME_DOMAIN_PLAYER && target.id < session->state.player_count) {
        node = session->state.players[target.id].node;
        use->victim_player = target.id;
        use->target_x = session->presentation.actors[target.id].visual_x;
        use->target_y = session->presentation.actors[target.id].visual_y;
        use->has_target_position = 1;
    } else if (target.kind == GAME_DOMAIN_SPECIAL_PLAYER && target.id < 4)
        node = session->state.special_player_node[target.id];
    else if (target.kind == GAME_DOMAIN_MAP_OBJECT && target.id < GAME_TOOL_PLACED_OBJECT_COUNT)
        node = session->state.tool_placed_objects[target.id].node;
    else if (target.kind == GAME_DOMAIN_GOD && target.id < GAME_MAP_DYNAMIC_OBJECT_COUNT)
        node = session->presentation.dynamic_objects[target.id].node;
    if (node > 0 && node < session->state.tool_route_node_count) {
        use->target_x = session->state.tool_route_x[node];
        use->target_y = session->state.tool_route_y[node];
        use->has_target_position = 1;
    }
    if (map == NULL || target.id >= GAME_MAX_MAP_PROPERTIES) return;
    if (target.kind == GAME_DOMAIN_LAND && map->lands != NULL &&
        target.id < session->state.land_count && (size_t)(target.id + 1) < map->land_count) {
        use->target_x = map->lands[target.id + 1].x;
        use->target_y = map->lands[target.id + 1].y;
        use->victim_player = session->state.land_owner[target.id];
    } else if (target.kind == GAME_DOMAIN_FACILITY && map->facilities != NULL &&
        target.id < session->state.facility_count && (size_t)(target.id + 1) < map->facility_count) {
        use->target_x = map->facilities[target.id + 1].x;
        use->target_y = map->facilities[target.id + 1].y;
        use->victim_player = session->state.facility_owner[target.id];
    } else return;
    use->has_target_position = 1;
}

/* 0x475D60 handlers index the same 12 x 3 x 30 speech table. */
static int caster_type(int card)
{
    return card == 9 || card == 10 || card == 11 ||
        (card >= 21 && card <= 27) ? 0 : 3;
}

static void card_say(GameSession *session, int player, int role, int card,
    int type, uint32_t now)
{
    if (player >= 0 && player < session->state.player_count)
        (void)game_start_speech_entry(session, player,
            rich4_character_card_speech(session->state.players[player].character,
                role, card), type, session->presentation.property.phase, now);
}

static int card_cash_shortfall(const GameSession *session,
    const GameCardUsePresentationState *use)
{
    const GameCommand *command = &use->command;
    int id = command->target.id, price;
    if (command->subject.id != 2 || use->victim_player < 0 ||
        use->victim_player == command->player ||
        !rich4_card_inventory_has(&session->state, command->player, 2)) return 0;
    if (command->target.kind == GAME_DOMAIN_FACILITY && id >= 0 && id < session->state.facility_count) {
        price = session->state.facility_value[id];
        if (price <= 0) price = session->state.facility_price[id];
    } else if (command->target.kind == GAME_DOMAIN_LAND && id >= 0 && id < session->state.land_count) {
        price = session->state.land_value[id];
        if (price <= 0) price = session->state.land_price[id] +
            session->state.land_level[id] * session->state.land_price_per_level[id];
    } else return 0;
    return session->state.players[command->player].cash < price;
}

static int card_commits_before_flight(const GameCardUsePresentationState *use)
{
    int card = use->command.subject.id;
    return card == 1 || card == 9 || card == 26 || card == 27 ||
        (card == 8 && use->command.target.kind == GAME_DOMAIN_LAND);
}

static int card_has_flight(int card)
{
    /* 0x442325/0x44309B/0x443225 have no 0x40E669 call. */
    return card != 2 && card != 6 && card != 7;
}

/* These handlers call 0x441343 before the caster speaks. Keep the hand
 * and public supply accurate throughout the blocking speech/animation.
 * At dispatch, reconstruct the input hand for the normal atomic command:
 * it applies the effect and debit together, or leaves both rolled back. */
static void restore_card_for_dispatch(GameSession *session)
{
    GameCardUsePresentationState *use = &session->presentation.card_use;
    if (use->reserved_card_slot) {
        int slot = use->reserved_card_slot - 1;
        uint8_t *hand = session->state.players[use->command.player].card_inventory;
        memmove(hand + slot + 1, hand + slot, GAME_PLAYER_CARD_SLOTS - slot - 1);
        hand[slot] = (uint8_t)(use->command.subject.id + 1);
        --session->state.card_amount[use->command.subject.id];
        use->reserved_card_slot = 0;
    }
}

static Rich4CardUseStep card_command_ready(GameSession *session)
{
    restore_card_for_dispatch(session);
    session->presentation.card_use.phase = GAME_CARD_USE_COMMAND;
    return RICH4_CARD_USE_COMMAND;
}

static int consumes_before_caster_speech(int card)
{
    switch (card + 1) {
    case 1: case 2: case 6: case 9: case 10: case 11: case 12:
    case 14: case 15: case 16: case 17: case 23:
    case 26: case 27: case 28: case 29: case 30:
        return 1;
    default:
        return 0;
    }
}

static void card_begin_handler(GameSession *session, uint32_t now)
{
    GameCardUsePresentationState *use = &session->presentation.card_use;
    int card = use->command.subject.id;
    use->phase = GAME_CARD_USE_COMMAND;
    if (rich4_card_inspect(&session->state, &use->command) != GAME_CARD_USE_APPLIED) return;
    if (card >= 17 && card <= 20) return;
    if (card_cash_shortfall(session, use)) return;
    if (card == 6 && ((use->command.target.kind == GAME_DOMAIN_LAND &&
        session->state.land_level[use->command.target.id] == 0) ||
        (use->command.target.kind == GAME_DOMAIN_FACILITY &&
         session->state.facility_level[use->command.target.id] == 0))) return;
    /* 0x444C45 releases the god/bomb before the caster's closing speech. */
    if (card == 21) return;
    if (card == 8 && use->command.source == GAME_COMMAND_SOURCE_HUMAN &&
        use->command.target.kind == GAME_DOMAIN_FACILITY &&
        session->state.facility_level[use->command.target.id] == 0 && !use->skip_caster) {
        use->resume_panel_phase = CARD_PANEL_FACILITY_CHOICE;
        session->presentation.card_panel.primary_target = use->command.target.id;
        session->presentation.card_panel.primary_kind = GAME_DOMAIN_FACILITY;
    }
    if (use->command.source == GAME_COMMAND_SOURCE_HUMAN &&
        consumes_before_caster_speech(card)) {
        use->reserved_card_slot = rich4_card_consume(&session->state, use->command.player, card);
        if (!use->reserved_card_slot) return;
    }
    use->phase = GAME_CARD_USE_CASTER_SPEECH;
    if (!use->skip_caster && !((card == 13 || card == 29) &&
        use->command.target.kind == GAME_DOMAIN_PLAYER &&
        use->command.target.id == use->command.player))
        card_say(session, card == 3 || card == 4 ? use->exchange_caster : use->command.player,
            0, card, caster_type(card), now);
}

int rich4_card_use_begin(GameSession *session, const GameCommand *command,
    const MapData *map, int show_notice, uint32_t now)
{
    GameCardUsePresentationState *use;
    if (session == NULL || command == NULL ||
        !rich4_game_command_is_valid(command) ||
        command->type != GAME_COMMAND_USE_CARD ||
        command->subject.kind != GAME_DOMAIN_CARD ||
        command->subject.id < 0 || command->subject.id >= GAME_CARD_COUNT ||
        command->player < 0 || command->player >= session->state.player_count ||
        session->presentation.card_use.phase != GAME_CARD_USE_NONE)
        return 0;
    use = &session->presentation.card_use;
    if (use->reserved_card_slot) {
        /* The angel's mandatory building choice continues the same use. */
        if (use->command.subject.id != command->subject.id || use->command.player != command->player ||
            use->command.target.kind != command->target.kind || use->command.target.id != command->target.id)
            return 0;
        restore_card_for_dispatch(session);
    }
    memset(use, 0, sizeof(*use));
    use->command = *command;
    use->skip_caster = command->source == GAME_COMMAND_SOURCE_HUMAN &&
        session->presentation.card_panel.effect_precedes_handler == 2;
    use->started = now;
    use->victim_player = -1;
    use->flight_sprite_index = 19;
    card_target_position(use, session, map);
    rich4_card_effect_prepare(session);
    if (command->subject.id == 4 && map != NULL && command->value >= 0) {
        int source = command->value;
        if (command->target.kind == GAME_DOMAIN_LAND && map->lands && (size_t)(source + 1) < map->land_count) {
            use->swap_from_x = map->lands[source + 1].x;
            use->swap_from_y = map->lands[source + 1].y;
        } else if (command->target.kind == GAME_DOMAIN_FACILITY && map->facilities &&
                   (size_t)(source + 1) < map->facility_count) {
            use->swap_from_x = map->facilities[source + 1].x;
            use->swap_from_y = map->facilities[source + 1].y;
        }
    }
    if (command->subject.id == 25 && use->victim_player >= 0)
        use->amount = session->state.players[use->victim_player].cash / 5;
    if (show_notice) {
        use->phase = GAME_CARD_USE_NOTICE;
        use->sound_pending = 1;
    } else card_begin_handler(session, now);
    return 1;
}

int rich4_card_use_begin_selection(GameSession *session, int player, int card,
    GameDomainRef target, const MapData *map, int panel_phase, uint32_t now)
{
    GameCommand command = rich4_game_domain_command(GAME_COMMAND_USE_CARD,
        GAME_COMMAND_SOURCE_HUMAN, player, rich4_game_domain_ref(GAME_DOMAIN_CARD, card),
        target, 0);
    if (!rich4_card_use_begin(session, &command, map, 0, now)) return 0;
    session->presentation.card_use.resume_panel_phase = panel_phase;
    session->presentation.card_use.phase = GAME_CARD_USE_CASTER_SPEECH;
    if (!session->presentation.speech.active)
        card_say(session, player, 0, card, caster_type(card), now);
    session->presentation.card_panel.phase = CARD_PANEL_WAIT_HANDLER;
    return 1;
}

int rich4_card_use_begin_stock_selection(GameSession *session,
    int player, int card, uint32_t now)
{
    if (card != 23 && card != 24) return 0;
    return rich4_card_use_begin_selection(session, player, card,
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), NULL, CARD_PANEL_STOCK_TARGET, now);
}

int rich4_card_use_skip_notice(GameSession *session, uint32_t now)
{
    if (session == NULL) return 0;
    if (session->presentation.card_use.phase == GAME_CARD_USE_NOTICE) {
        session->presentation.card_use.skip_requested = 1;
        return 1;
    }
    if (session->presentation.card_use.defense.phase == GAME_PROPERTY_PRESENT_INSURANCE_CARD ||
        session->presentation.card_use.defense.phase == GAME_PROPERTY_PRESENT_INSURANCE_REDIRECT_NOTICE) {
        session->presentation.card_use.defense.skip_requested = 1;
        return 1;
    }
    if (session->presentation.card_panel.phase == CARD_PANEL_EFFECT) {
        session->presentation.card_panel.effect_until = now;
        return 1;
    }
    return 0;
}

Rich4CardUseStep rich4_card_use_update(GameSession *session,
    const GameViewportModel *viewport, uint32_t now)
{
    GameCardUsePresentationState *use;
    if (session == NULL) return RICH4_CARD_USE_FINISHED;
    use = &session->presentation.card_use;
    switch (use->phase) {
    case GAME_CARD_USE_NOTICE:
        if (!use->skip_requested && now - use->started < RICH4_CARD_NOTICE_MS)
            return RICH4_CARD_USE_WAIT;
        card_begin_handler(session, now);
        return RICH4_CARD_USE_WAIT;
    case GAME_CARD_USE_CASTER_SPEECH:
        if (session->presentation.speech.active) return RICH4_CARD_USE_WAIT;
        if (use->resume_panel_phase && use->command.subject.id != 12 && use->command.subject.id != 8) {
            session->presentation.card_panel.phase = use->resume_panel_phase;
            session->presentation.card_panel.effect_precedes_handler = 2;
            use->phase = GAME_CARD_USE_DONE;
            return RICH4_CARD_USE_FINISHED;
        }
        if (use->command.source == GAME_COMMAND_SOURCE_HUMAN &&
            use->command.target.kind == GAME_DOMAIN_STOCK &&
            (use->command.subject.id == 23 || use->command.subject.id == 24)) {
            int stock = use->command.target.id;
            int history = rich4_stock_latest_history_index(&session->state);
            use->previous_quote = session->state.stocks[stock];
            use->previous_history_price = session->state.stock_history[stock][history];
            if (!rich4_stock_apply_card_quote(&session->state, stock, use->command.subject.id == 23))
                return card_command_ready(session);
            /* 0x42B137 refreshes the quote page before the blocking 1000 ms wait. */
            use->started = now;
            use->phase = GAME_CARD_USE_STOCK_HOLD;
            return RICH4_CARD_USE_WAIT;
        }
        if (use->command.subject.id == 23 || use->command.subject.id == 24 ||
            (use->skip_caster && (use->command.subject.id == 12 || use->command.subject.id == 8))) {
            if (use->command.subject.id == 8) use->flash_pending = 0;
            return card_command_ready(session);
        }
        if (card_commits_before_flight(use)) {
            return card_command_ready(session);
        }
        use->started = now;
        use->phase = GAME_CARD_USE_FLIGHT;
        /* fall through */
    case GAME_CARD_USE_FLIGHT:
        if (use->has_target_position && card_has_flight(use->command.subject.id) &&
            (use->command.subject.id == 22 || use->command.source == GAME_COMMAND_SOURCE_AI ||
             (use->command.subject.id == 8 && use->command.target.kind == GAME_DOMAIN_LAND))) {
            if (viewport == NULL) return RICH4_CARD_USE_WAIT;
            if (!use->flight_initialized) {
                int player = use->command.player;
                int node = session->state.players[player].node;
                int x = session->presentation.actors[player].visual_x;
                int y = session->presentation.actors[player].visual_y;
                int dx, dy;
                long squared;
                int distance;
                if (node > 0 && node < session->state.tool_route_node_count) {
                    x = session->state.tool_route_x[node];
                    y = session->state.tool_route_y[node];
                }
                rich4_game_viewport_project(viewport, x, y,
                    &use->flight_from_x, &use->flight_from_y);
                rich4_game_viewport_project(viewport, use->target_x, use->target_y,
                    &use->flight_to_x, &use->flight_to_y);
                if (use->command.subject.id == 22) {
                    int temp = use->flight_from_x;
                    use->flight_from_x = use->flight_to_x; use->flight_to_x = temp;
                    temp = use->flight_from_y;
                    use->flight_from_y = use->flight_to_y; use->flight_to_y = temp;
                    use->flight_sprite_index = use->command.value - 1;
                    use->flight_sprite_chunk = (session->presentation.dynamic_objects[
                        use->command.target.id].node_direction + 8 - viewport->direction) & 7;
                }
                dx = use->flight_to_x - use->flight_from_x;
                dy = use->flight_to_y - use->flight_from_y;
                squared = (long)dx * dx + (long)dy * dy;
                distance = rich4_int_sqrt_floor(squared);
                /* 0x40E74F's __CHP (0x457DBC) temporarily sets x87 RC=11:
                 * truncate(sqrt(squared)/8 + 1), not nearest rounding. */
                use->flight_steps = dx || dy ? distance / 8 + 1 : 0;
                use->flight_initialized = 1;
                if (use->flight_steps > 0) {
                    use->flight_dx = (float)dx / use->flight_steps;
                    use->flight_dy = (float)dy / use->flight_steps;
                    use->flight_x = use->flight_from_x + use->flight_dx;
                    use->flight_y = use->flight_from_y + use->flight_dy;
                    use->flight_step = 1;
                    use->flight_tick = now;
                    return RICH4_CARD_USE_WAIT;
                }
            }
            if (use->flight_steps > 0) {
                /* 0x40E81A..0x40E9B4 times each draw separately. A slow
                 * frame never skips the intermediate displayed positions. */
                if (use->flight_step <= use->flight_steps) {
                    if (now - use->flight_tick < 24u) return RICH4_CARD_USE_WAIT;
                    use->flight_tick = now;
                    use->flight_step++;
                    if (use->flight_step <= use->flight_steps) {
                        use->flight_x += use->flight_dx;
                        use->flight_y += use->flight_dy;
                        return RICH4_CARD_USE_WAIT;
                    }
                    use->started = now;
                }
                if (now - use->started < (use->command.subject.id == 22 ? 0u : 100u))
                    return RICH4_CARD_USE_WAIT;
            }
        }
        if (use->accepted) {
            if (use->flash_pending) rich4_card_effect_flash(session, now);
            use->phase = GAME_CARD_USE_EFFECTS;
            return RICH4_CARD_USE_WAIT;
        }
        if (use->resume_panel_phase && use->command.subject.id == 8) {
            rich4_card_effect_flash(session, now);
            use->phase = GAME_CARD_USE_BEFORE_COMMAND_FLASH;
            return RICH4_CARD_USE_WAIT;
        }
        if (use->resume_panel_phase) {
            session->presentation.card_panel.phase = use->resume_panel_phase;
            session->presentation.card_panel.effect_precedes_handler = 2;
            use->phase = GAME_CARD_USE_DONE;
            return RICH4_CARD_USE_FINISHED;
        }
        if (use->has_target_position &&
            (use->command.subject.id == 10 || use->command.subject.id == 11))
            game_focus_view_at(session, use->target_x, use->target_y);
        if ((use->command.subject.id == 4 ||
             (use->command.subject.id == 8 && use->command.target.kind == GAME_DOMAIN_FACILITY)) &&
            use->flash_pending) {
            rich4_card_effect_flash(session, now);
            use->phase = GAME_CARD_USE_BEFORE_COMMAND_FLASH;
            return RICH4_CARD_USE_WAIT;
        }
        /* fall through */
    case GAME_CARD_USE_BEFORE_COMMAND_FLASH:
        if (rich4_card_effect_flash_update(session, now)) return RICH4_CARD_USE_WAIT;
        if (use->resume_panel_phase) {
            session->presentation.card_panel.phase = use->resume_panel_phase;
            session->presentation.card_panel.effect_precedes_handler = 2;
            use->phase = GAME_CARD_USE_DONE;
            return RICH4_CARD_USE_FINISHED;
        }
        if (use->command.subject.id == 4) {
            int dx = use->target_x - use->swap_from_x;
            int dy = use->target_y - use->swap_from_y;
            use->swap_steps = rich4_int_sqrt_floor((long)dx*dx + (long)dy*dy) / 16 + 1;
            use->swap_dx = (float)dx / use->swap_steps;
            use->swap_dy = (float)dy / use->swap_steps;
            use->swap_x = use->swap_dx;
            use->swap_y = use->swap_dy;
            use->swap_step = 1;
            use->swap_tick = now;
            use->phase = GAME_CARD_USE_SWAP_HOUSES;
            return RICH4_CARD_USE_WAIT;
        }
        rich4_card_defense_begin(session, now);
        use->phase = GAME_CARD_USE_DEFENSE;
        /* fall through */
    case GAME_CARD_USE_DEFENSE:
        if (rich4_card_defense_update(session, now)) return RICH4_CARD_USE_WAIT;
        use->phase = GAME_CARD_USE_TARGET_SPEECH;
        if (!use->blocked) {
            int card = use->command.subject.id;
            if (card == 15) {
                int victim = use->victim_player;
                if (victim >= 0) (void)game_start_speech_entry(session, victim,
                    rich4_character_sleepwalking_speech(session->state.players[victim].character),
                    1, session->presentation.property.phase, now);
            } else if (card == 7) {
                if (use->victim_player != use->command.player)
                    card_say(session, use->victim_player, 2, 7, 1, now);
            } else if (card == 13 || card == 29) {
                int self = use->victim_player == use->command.player;
                card_say(session, use->victim_player, self ? 1 : 2, card,
                    self ? 3 : card == 7 ? 1 : 2, now);
            }
        }
        if (session->presentation.speech.active) return RICH4_CARD_USE_WAIT;
        /* fall through */
    case GAME_CARD_USE_TARGET_SPEECH:
        if (session->presentation.speech.active) return RICH4_CARD_USE_WAIT;
        return card_command_ready(session);
    case GAME_CARD_USE_SWAP_HOUSES:
        if (now - use->swap_tick < 24u) return RICH4_CARD_USE_WAIT;
        use->swap_tick = now;
        if (++use->swap_step <= use->swap_steps) {
            use->swap_x += use->swap_dx;
            use->swap_y += use->swap_dy;
            return RICH4_CARD_USE_WAIT;
        }
        use->phase = GAME_CARD_USE_COMMAND;
        /* fall through */
    case GAME_CARD_USE_COMMAND:
        return card_command_ready(session);
    case GAME_CARD_USE_EFFECTS:
        if (rich4_card_effect_flash_update(session, now)) return RICH4_CARD_USE_WAIT;
        if (use->accepted && use->max_level_pending) return RICH4_CARD_USE_MAX_LEVEL;
        {
            Rich4PresentationActivity activity = rich4_presentation_activity_from_state(
                &session->presentation, &session->state);
            if (rich4_presentation_mode_resolve(&activity) != RICH4_PRESENTATION_NONE ||
                session->presentation.god_detachment.active)
                return RICH4_CARD_USE_DISPATCH_EFFECTS;
        }
        if (use->accepted && !use->blocked && use->command.subject.id == 16 &&
            !use->reaction_spoken) {
            use->reaction_spoken = 1;
            card_say(session, use->victim_player, 2, 16, 1, now);
            return RICH4_CARD_USE_WAIT;
        }
        if (use->accepted && (use->command.subject.id == 3 || use->command.subject.id == 4) &&
            !use->finish_hold_done) {
            rich4_presentation_clear_view_override(&session->presentation);
            use->phase = GAME_CARD_USE_FINISH_HOLD;
            use->started = now;
            return RICH4_CARD_USE_WAIT;
        }
        if (use->accepted && use->revenge_pending && !use->revenge_committed) {
            rich4_card_defense_revenge_begin(session, now);
            use->phase = GAME_CARD_USE_REVENGE;
            return RICH4_CARD_USE_WAIT;
        }
        use->phase = GAME_CARD_USE_VICTIM_SPEECH;
        if (use->accepted && !use->blocked) {
            int card = use->command.subject.id;
            int type = card == 25 || card == 3 || card == 4 ? 2 : card == 28 ? 0 : 1;
            if (card == 1 || (card >= 2 && card <= 4) || card == 10 || card == 11 ||
                card == 12 || (card == 25 && use->victim_player != use->command.player) ||
                card == 28)
                card_say(session, card == 3 || card == 4 ? use->exchange_victim : use->victim_player,
                    2, card, type, now);
        }
        if (use->accepted && use->command.subject.id == 5) {
            int self = use->victim_player == use->command.player;
            card_say(session, use->victim_player, self ? 1 : 2, 5, self ? 0 : 2, now);
        }
        if (use->accepted && use->command.subject.id == 21)
            card_say(session, use->command.player, 0, 21, 0, now);
        return RICH4_CARD_USE_WAIT;
    case GAME_CARD_USE_STOCK_HOLD: {
        int stock = use->command.target.id;
        int history = rich4_stock_latest_history_index(&session->state);
        if (now - use->started < 1000u) return RICH4_CARD_USE_WAIT;
        /* Replay the pre-quote state through the atomic command at the
         * original debit point. Failure restores both the hand and quote. */
        session->state.stocks[stock] = use->previous_quote;
        session->state.stock_history[stock][history] = use->previous_history_price;
        return card_command_ready(session);
    }
    case GAME_CARD_USE_FINISH_HOLD:
        if (now - use->started < 500u) return RICH4_CARD_USE_WAIT;
        use->finish_hold_done = 1;
        use->phase = GAME_CARD_USE_EFFECTS;
        return RICH4_CARD_USE_WAIT;
    case GAME_CARD_USE_REVENGE:
        if (rich4_card_defense_update(session, now)) return RICH4_CARD_USE_WAIT;
        return RICH4_CARD_USE_REVENGE_COMMAND;
    case GAME_CARD_USE_VICTIM_SPEECH:
        if (session->presentation.speech.active) return RICH4_CARD_USE_WAIT;
        rich4_presentation_clear_view_override(&session->presentation);
        use->phase = GAME_CARD_USE_DONE;
        /* fall through */
    case GAME_CARD_USE_DONE:
    case GAME_CARD_USE_NONE:
        return RICH4_CARD_USE_FINISHED;
    default:
        return RICH4_CARD_USE_WAIT;
    }
}

void rich4_card_use_command_completed(GameSession *session, int accepted, uint32_t now)
{
    if (session != NULL && session->presentation.card_use.phase == GAME_CARD_USE_COMMAND) {
        const GameCommand *command = &session->presentation.card_use.command;
        session->presentation.card_use.accepted = accepted;
        session->presentation.card_use.phase = GAME_CARD_USE_EFFECTS;
        if (accepted && card_commits_before_flight(&session->presentation.card_use)) {
            session->presentation.card_use.phase = GAME_CARD_USE_FLIGHT;
            return;
        }
        if (accepted && session->presentation.card_use.flash_pending)
            rich4_card_effect_flash(session, now);
        /* 0x44536F compares the final payer with the caster before the
         * payment message and victim speech.  A misdirect back to the
         * caster skips both. */
        if (accepted && command->subject.id == 25 &&
            !session->presentation.card_use.blocked &&
            session->presentation.card_use.victim_player != command->player) {
            GameCardUsePresentationState *use = &session->presentation.card_use;
            GameEvent notice = rich4_game_event_special_place_notice(command->player,
                GAME_SPECIAL_NOTICE_CARD_TAX, rich4_game_domain_ref(GAME_DOMAIN_PLAYER,
                    use->victim_player), use->amount, RICH4_CARD_NOTICE_MS);
            (void)rich4_special_place_notice_enqueue(session, &notice, now);
        }
        if (!accepted && card_cash_shortfall(session, &session->presentation.card_use)) {
            GameEvent notice = rich4_game_event_special_place_notice(command->player,
                GAME_SPECIAL_NOTICE_CARD_CASH_SHORTFALL, command->target, 0, RICH4_CARD_NOTICE_MS);
            (void)rich4_special_place_notice_enqueue(session, &notice, now);
        }
        if (accepted && command->source == GAME_COMMAND_SOURCE_AI &&
            (command->subject.id == 23 || command->subject.id == 24)) {
            GameEvent notice = rich4_game_event_special_place_notice(
                command->player, GAME_SPECIAL_NOTICE_STOCK_CARD,
                command->target, command->subject.id, RICH4_CARD_NOTICE_MS);
            (void)rich4_special_place_notice_enqueue(session, &notice, now);
        }
    }
}
