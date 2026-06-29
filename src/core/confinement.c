#include "core/confinement.h"

#include <limits.h>
#include <string.h>

#include "core/insurance.h"
#include "core/game_event_payload.h"

static int push_confinement_event(GameEventQueue *events,
                                  Rich4GameEventConstructor constructor,
                                  int player, int subject, int value,
                                  int value2)
{
    Rich4GameEventFields fields;
    fields.player = player;
    fields.subject = subject;
    fields.value = value;
    fields.value2 = value2;
    fields.flags = 0u;
    fields.subject_ref.kind = GAME_DOMAIN_NONE;
    fields.subject_ref.id = -1;
    fields.target_ref.kind = GAME_DOMAIN_NONE;
    fields.target_ref.id = -1;
    return rich4_game_event_emit_typed(events, constructor, fields);
}

static int special_node(const GameState *state, int kind)
{
    return kind == GAME_CONFINEMENT_PRISON ? state->prison_node : state->hospital_node;
}

void rich4_confinement_clear_status(GamePlayerState *player)
{
    /* 0x40D761 clears ordinary actor record +50..+53. */
    if (player == NULL) return;
    player->delayed_status_block = 0;
    player->days_disappearing = player->days_in_prison = player->days_in_hospital = 0;
    player->confinement_actor_type = GAME_CONFINEMENT_NONE;
}

void rich4_confinement_injure_player(GameState *state, int actor)
{
    GamePlayerState *player;
    int method;
    /* 0x40CD07, shared by explosions, alien attacks and injury fortunes. */
    if (state == NULL || actor < 0 || actor >= state->player_count) return;
    player = &state->players[actor];
    if ((player->play_state & GAME_PLAYER_PLAY_STATE_MASK) == 0 ||
        player->delayed_status_block || player->days_disappearing ||
        player->days_in_prison || player->days_in_hospital) return;
    method = player->traffic_method & 3;
    if (method == 1 || method == 2) state->tool_amount[method + 3]++;
    if (player->traffic_method != 0) {
        player->traffic_method = 0;
        player->ndices = 1;
    }
    player->play_state |= GAME_PLAYER_PLAY_STATE_FROZEN;
}

static int emit_transition(GameEventQueue *events, int actor, int kind, int days,
                           int entering)
{
    int offset = kind == GAME_CONFINEMENT_PRISON ? 52 : 53;
    return push_confinement_event(events,
                                  rich4_game_event_status_changed_from_fields,
                                  actor, offset, days, entering) &&
           push_confinement_event(
               events, rich4_game_event_actor_dirty_requested_from_fields,
               actor, offset, kind, entering) &&
           push_confinement_event(
               events, rich4_game_event_scene_refresh_requested_from_fields,
               actor, offset, kind, entering) &&
           (!entering || push_confinement_event(
               events, rich4_game_event_text_requested_from_fields,
               actor, offset, kind, days)) &&
           (!entering || push_confinement_event(
               events, rich4_game_event_voice_requested_from_fields,
               actor, offset, kind, days));
}

static void apply_rescue_relation_penalty(GameState *state, int rescued,
                                          int visitor, int days)
{
    int64_t penalty;
    int64_t value;
    GamePlayerState *player;
    if (state == NULL || rescued < 0 || rescued >= state->player_count ||
        visitor < 0 || visitor >= GAME_PLAYER_RELATION_BALANCE_COUNT ||
        rescued == visitor || days <= 0 || state->price_index <= 0) return;
    player = &state->players[rescued];
    if (player->relation_balance[visitor] == 0) return;
    /* sub_40DF69(target, visitor, -(price_index * 100 * remaining_days))
     * subtracts from the rescued player's balance and clamps at zero. */
    penalty = (int64_t)state->price_index * 100 * days;
    value = (int64_t)player->relation_balance[visitor] - penalty;
    player->relation_balance[visitor] = value > 0 ? (int)value : 0;
}

static GameConfinementResult confinement_enter(GameState *state, int actor,
                                               int kind, int days,
                                               GameEventQueue *events, int defer_claim)
{
    GameState next;
    GameEventQueue staged;
    int node;
    if (state == NULL || events == NULL || actor < 0 || actor >= 8 ||
        (kind != GAME_CONFINEMENT_PRISON && kind != GAME_CONFINEMENT_HOSPITAL) ||
        days <= 0 || days >= GAME_STATUS_PENDING_BIT) return GAME_CONFINEMENT_INVALID;
    node = special_node(state, kind);
    if (node < 0 || node >= GAME_MAX_MAP_ROUTE_NODES) return GAME_CONFINEMENT_INVALID;
    next = *state;
    rich4_game_event_queue_reset(&staged);
    if (actor < GAME_MAX_PLAYERS) {
        GamePlayerState *p;
        int *counter;
        int current;
        if (actor >= state->player_count) return GAME_CONFINEMENT_INVALID;
        p = &next.players[actor];
        counter = kind == GAME_CONFINEMENT_PRISON ? &p->days_in_prison : &p->days_in_hospital;
        current = *counter & 0xff;
        if (current != 0) {
            *counter = (current + days) & 0x7f;
        } else {
            rich4_confinement_clear_status(p);
            p->node = node;
            p->previous_node = 0;
            p->next_node = 0;
            p->play_state &= GAME_PLAYER_PLAY_STATE_MASK;
            p->confinement_actor_type = kind;
            *counter = days;
        }
        if (!defer_claim)
            p->total_confinement_days = (p->total_confinement_days + days) & 0xff;
    } else {
        int special = actor - 4;
        if (special < 0 || special >= GAME_SPECIAL_PLAYER_COUNT) return GAME_CONFINEMENT_INVALID;
        next.special_player_confinement[special] = kind;
        /* 0x43D593 writes record +10 to 1; 0x43EC3F writes it to 2.
         * Nonzero removes the actor from automatic turn rotation. */
        next.special_player_f10[special] = kind;
        next.special_player_actor_type[special] = 0;
        memset(next.special_player_status[special], 0,
               sizeof(next.special_player_status[special]));
    }
    if (!emit_transition(&staged, actor, kind, days, 1) ||
        (!defer_claim && actor < GAME_MAX_PLAYERS &&
         !rich4_insurance_claim_for_days(
             &next, actor, days,
             kind == GAME_CONFINEMENT_PRISON ?
                 GAME_INSURANCE_CLAIM_PRISON :
                 GAME_INSURANCE_CLAIM_HOSPITAL,
             NULL, &staged)) ||
        !rich4_game_event_queue_append(events, &staged))
        return GAME_CONFINEMENT_EVENT_CAPACITY;
    *state = next;
    return GAME_CONFINEMENT_APPLIED;
}

GameConfinementResult rich4_confinement_enter(GameState *state, int actor,
    int kind, int days, GameEventQueue *events)
{
    return confinement_enter(state, actor, kind, days, events, 0);
}

/* 0x40D375, shared by aviation, 0x44C5D8 and 0x44C6ED. Only the first
 * disappearance invokes 0x44BA63 and adds days to player +66. */
GameConfinementResult rich4_confinement_disappear(GameState *state, int actor,
    int days, int alien, int defer_claim, GameEventQueue *events)
{
    GameState next;
    GameEventQueue staged;
    GamePlayerState *p;
    int current, first, count;
    if (state == NULL || events == NULL || actor < 0 ||
        actor >= state->player_count || days <= 0 || days > 0x3f ||
        (alien != 0 && alien != 1)) return GAME_CONFINEMENT_INVALID;
    next = *state;
    p = &next.players[actor];
    current = p->days_disappearing & 0xff;
    first = current == 0;
    rich4_game_event_queue_reset(&staged);
    if (first) {
        rich4_confinement_clear_status(p);
        count = staged.count;
        if (!(defer_claim ? rich4_insurance_request_claim_for_days(
                &next, actor, days, GAME_INSURANCE_CLAIM_DISAPPEARING, &staged) :
              rich4_insurance_claim_for_days(&next, actor, days,
                GAME_INSURANCE_CLAIM_DISAPPEARING, NULL, &staged)))
            return GAME_CONFINEMENT_EVENT_CAPACITY;
        if (!defer_claim || staged.count == count)
            p->total_confinement_days = (p->total_confinement_days + days) & 0xff;
    }
    p->days_disappearing = ((current & 0x3f) + days + (alien ? 0x40 : 0)) & 0xff;
    if (!rich4_game_event_emit_typed_domain(&staged,
            rich4_game_event_status_changed_from_fields, actor,
            rich4_game_domain_ref(GAME_DOMAIN_STATUS, GAME_STATUS_DISAPPEARING),
            rich4_game_domain_ref(GAME_DOMAIN_PLAYER, actor),
            p->days_disappearing, 0, 0) ||
        !rich4_game_event_queue_append(events, &staged))
        return GAME_CONFINEMENT_EVENT_CAPACITY;
    *state = next;
    return GAME_CONFINEMENT_APPLIED;
}

/* 0x43D593/0x43EC3F call 0x44BA63 after animation and speech. */
GameConfinementResult rich4_confinement_enter_deferred_claim(
    GameState *state, int actor, int kind, int days, GameEventQueue *events)
{
    return confinement_enter(state, actor, kind, days, events, 1);
}

GameConfinementResult rich4_confinement_request_enter(
    GameState *state, int actor, int kind, int days, GameEventQueue *events)
{
    GameState next;
    GameEventQueue staged;
    GameConfinementResult result;
    if (state == NULL || events == NULL) return GAME_CONFINEMENT_INVALID;
    if (actor >= GAME_MAX_PLAYERS)
        return rich4_confinement_enter(state, actor, kind, days, events);
    next = *state;
    rich4_game_event_queue_reset(&staged);
    result = rich4_confinement_enter_deferred_claim(&next, actor, kind, days, &staged);
    if (result != GAME_CONFINEMENT_APPLIED) return result;
    return push_confinement_event(events, rich4_game_event_text_requested_from_fields,
        actor, kind == GAME_CONFINEMENT_PRISON ? 52 : 53, kind, days)
        ? GAME_CONFINEMENT_APPLIED : GAME_CONFINEMENT_EVENT_CAPACITY;
}

GameConfinementResult rich4_confinement_release(GameState *state, int actor,
                                                 int kind,
                                                 GameEventQueue *events)
{
    GameState next;
    GameEventQueue staged;
    if (state == NULL || events == NULL || actor < 0 || actor >= 8 ||
        (kind != GAME_CONFINEMENT_PRISON && kind != GAME_CONFINEMENT_HOSPITAL))
        return GAME_CONFINEMENT_INVALID;
    next = *state;
    rich4_game_event_queue_reset(&staged);
    if (actor < GAME_MAX_PLAYERS) {
        GamePlayerState *p;
        int *counter;
        if (actor >= state->player_count) return GAME_CONFINEMENT_INVALID;
        p = &next.players[actor];
        counter = kind == GAME_CONFINEMENT_PRISON ? &p->days_in_prison : &p->days_in_hospital;
        if ((*counter & GAME_STATUS_PENDING_BIT) == 0) return GAME_CONFINEMENT_INVALID;
        *counter = 0;
        p->confinement_actor_type = GAME_CONFINEMENT_NONE;
    } else {
        int special = actor - 4;
        int node = special_node(state, kind);
        if (special < 0 || special >= GAME_SPECIAL_PLAYER_COUNT ||
            state->special_player_confinement[special] != kind || node < 0)
            return GAME_CONFINEMENT_INVALID;
        next.special_player_node[special] = node;
        next.special_player_previous_node[special] = 0;
        next.special_player_actor_type[special] = kind;
        next.special_player_confinement[special] = GAME_CONFINEMENT_NONE;
        next.special_player_f10[special] = 0;
    }
    if (!emit_transition(&staged, actor, kind, 0, 0) ||
        !rich4_game_event_queue_append(events, &staged))
        return GAME_CONFINEMENT_EVENT_CAPACITY;
    *state = next;
    return GAME_CONFINEMENT_APPLIED;
}

int rich4_confinement_visit_cost(GameDomainRef target)
{
    if (target.kind == GAME_DOMAIN_PLAYER &&
        target.id >= 0 && target.id < GAME_MAX_PLAYERS)
        return 30;
    if (target.kind == GAME_DOMAIN_SPECIAL_PLAYER &&
        target.id >= 0 && target.id < GAME_SPECIAL_PLAYER_COUNT)
        return 300;
    return -1;
}

int rich4_confinement_visit_candidate(const GameState *state,
                                      GameDomainRef target, int kind)
{
    if (state == NULL ||
        (kind != GAME_CONFINEMENT_PRISON &&
         kind != GAME_CONFINEMENT_HOSPITAL))
        return 0;
    if (target.kind == GAME_DOMAIN_PLAYER) {
        const GamePlayerState *player;
        int days;
        if (target.id < 0 || target.id >= state->player_count) return 0;
        player = &state->players[target.id];
        days = kind == GAME_CONFINEMENT_PRISON
            ? player->days_in_prison : player->days_in_hospital;
        return (player->play_state & 0xff) != 0 &&
               (days & 0x7f) != 0;
    }
    return target.kind == GAME_DOMAIN_SPECIAL_PLAYER &&
           target.id >= 0 &&
           target.id < GAME_SPECIAL_PLAYER_COUNT &&
           state->special_player_confinement[target.id] == kind;
}

GameConfinementResult rich4_confinement_visit_release(
    GameState *state, int visitor, GameDomainRef target, int kind,
    GameEventQueue *events)
{
    GameState next;
    GameEventQueue staged;
    GameDomainRef status_ref;
    int cost = rich4_confinement_visit_cost(target);
    int offset = kind == GAME_CONFINEMENT_PRISON ? 52 : 53;
    if (state == NULL || events == NULL ||
        visitor < 0 || visitor >= state->player_count || cost < 0 ||
        !rich4_confinement_visit_candidate(state, target, kind) ||
        state->players[visitor].points < cost)
        return GAME_CONFINEMENT_INVALID;
    next = *state;
    rich4_game_event_queue_reset(&staged);
    next.players[visitor].points =
        (uint16_t)(next.players[visitor].points - cost);
    status_ref.kind = GAME_DOMAIN_STATUS;
    status_ref.id = kind == GAME_CONFINEMENT_PRISON
        ? GAME_STATUS_PRISON : GAME_STATUS_HOSPITAL;
    if (!rich4_game_event_emit_typed_domain(
            &staged, rich4_game_event_inventory_changed_from_fields, visitor,
            status_ref,
            target, -cost, next.players[visitor].points, 0))
        return GAME_CONFINEMENT_EVENT_CAPACITY;
    if (target.kind == GAME_DOMAIN_PLAYER) {
        GamePlayerState *released = &next.players[target.id];
        int *days = kind == GAME_CONFINEMENT_PRISON
            ? &released->days_in_prison : &released->days_in_hospital;
        int remaining_days = *days & 0x7f;
        /* 0x43D304/0x43E9A4 clear the occupancy slot and write 0x80 to
         * the status byte.  The normal pending-status consumer performs
         * the final actor transition on that player's turn. */
        apply_rescue_relation_penalty(&next, target.id, visitor,
                                      remaining_days);
        *days = GAME_STATUS_PENDING_BIT;
        if (!rich4_game_event_status_changed_emit(
                &staged, target.id, offset, GAME_STATUS_PENDING_BIT, 0, 0) ||
            !rich4_game_event_actor_dirty_requested_emit(
                &staged, target.id, offset, kind, 0, 0) ||
            !rich4_game_event_scene_refresh_requested_emit(
                &staged, target.id, offset, kind, 0, 0))
            return GAME_CONFINEMENT_EVENT_CAPACITY;
    } else {
        int actor = target.id + GAME_MAX_PLAYERS;
        if (rich4_confinement_release(
                &next, actor, kind, &staged) != GAME_CONFINEMENT_APPLIED)
            return GAME_CONFINEMENT_EVENT_CAPACITY;
        /* 0x43D7BF/0x43EE6E store the rescuer in record +8 and clear
         * record +10, which immediately joins the actor to turn rotation. */
        next.special_player_play_state[target.id] = visitor;
        next.special_player_f10[target.id] = 0;
        next.special_player_previous_node[target.id] = 0;
    }
    if (!rich4_game_event_queue_append(events, &staged))
        return GAME_CONFINEMENT_EVENT_CAPACITY;
    *state = next;
    return GAME_CONFINEMENT_APPLIED;
}

static void snapshot_player(GameStatusPlayerSnapshot *d, const GamePlayerState *s)
{
    d->node=s->node; d->next_node=s->next_node; d->previous_node=s->previous_node;
    d->play_state=s->play_state;
    d->status[0]=s->delayed_status_block; d->status[1]=s->days_disappearing;
    d->status[2]=s->days_in_prison; d->status[3]=s->days_in_hospital;
    d->status[4]=s->days_sleeping; d->status[5]=s->days_sleep_walking;
    d->status[6]=s->days_stopping; d->status[7]=s->days_tortoise_walking;
    d->status[8]=s->days_rejected_by_bank; d->status[9]=s->days_bank_no_loans;
    d->status[10]=s->god_status_days; d->status[11]=s->days_assurance;
    d->god_info=s->god_info; d->god_type=s->god_type;
    d->god_income_modifier=s->god_income_modifier;
    d->god_investment_modifier=s->god_investment_modifier;
    d->god_land_modifier=s->god_land_modifier;
    d->linked_player=s->linked_player;
    d->confinement_actor_type=s->confinement_actor_type;
    d->total_confinement_days=s->total_confinement_days;
}

static void restore_player(GamePlayerState *d, const GameStatusPlayerSnapshot *s)
{
    d->node=s->node; d->next_node=s->next_node; d->previous_node=s->previous_node;
    d->play_state=s->play_state;
    d->delayed_status_block=s->status[0]; d->days_disappearing=s->status[1];
    d->days_in_prison=s->status[2]; d->days_in_hospital=s->status[3];
    d->days_sleeping=s->status[4]; d->days_sleep_walking=s->status[5];
    d->days_stopping=s->status[6]; d->days_tortoise_walking=s->status[7];
    d->days_rejected_by_bank=s->status[8]; d->days_bank_no_loans=s->status[9];
    d->god_status_days=s->status[10]; d->days_assurance=s->status[11];
    d->god_info=s->god_info; d->god_type=s->god_type;
    d->god_income_modifier=s->god_income_modifier;
    d->god_investment_modifier=s->god_investment_modifier;
    d->god_land_modifier=s->god_land_modifier;
    d->linked_player=s->linked_player;
    d->confinement_actor_type=s->confinement_actor_type;
    d->total_confinement_days=s->total_confinement_days;
}

int rich4_status_snapshot_export(const GameState *state, GameStatusSnapshot *out)
{
    int i;
    if (state == NULL || out == NULL) return 0;
    memset(out, 0, sizeof(*out));
    out->version=GAME_STATUS_SNAPSHOT_VERSION; out->player_count=state->player_count;
    out->prison_node=state->prison_node; out->hospital_node=state->hospital_node;
    for (i=0;i<GAME_MAX_PLAYERS;i++) snapshot_player(&out->players[i],&state->players[i]);
    memcpy(out->special_node,state->special_player_node,sizeof(out->special_node));
    memcpy(out->special_actor_type,state->special_player_actor_type,sizeof(out->special_actor_type));
    memcpy(out->special_confinement,state->special_player_confinement,sizeof(out->special_confinement));
    return 1;
}

int rich4_status_snapshot_import(GameState *state, const GameStatusSnapshot *in)
{
    GameState next;
    int i;
    if (state == NULL || in == NULL || in->version != GAME_STATUS_SNAPSHOT_VERSION ||
        in->player_count != state->player_count || in->player_count < 0 ||
        in->player_count > GAME_MAX_PLAYERS || in->prison_node < 0 ||
        in->hospital_node < 0) return 0;
    for (i=0;i<in->player_count;i++) {
        int j, k;
        if (in->players[i].node < 0 || in->players[i].node >= GAME_MAX_MAP_ROUTE_NODES ||
            in->players[i].god_info < 0 || in->players[i].god_info > GAME_MAP_DYNAMIC_OBJECT_COUNT ||
            in->players[i].god_type < 0 || in->players[i].god_type > GAME_GOD_TYPE_COUNT ||
            ((in->players[i].god_info == 0) != (in->players[i].god_type == 0)) ||
            in->players[i].linked_player < -1 || in->players[i].linked_player >= in->player_count)
            return 0;
        for (k=0;k<i;k++)
            if (in->players[i].god_info != 0 &&
                in->players[i].god_info == in->players[k].god_info) return 0;
        for (j=0;j<12;j++) if (in->players[i].status[j] < 0 || in->players[i].status[j] > 255) return 0;
    }
    for (i=0;i<GAME_SPECIAL_PLAYER_COUNT;i++)
        if (in->special_node[i] < -1 ||
            in->special_node[i] >= GAME_MAX_MAP_ROUTE_NODES ||
            in->special_actor_type[i] < GAME_CONFINEMENT_NONE ||
            in->special_actor_type[i] > GAME_CONFINEMENT_HOSPITAL ||
            in->special_confinement[i] < GAME_CONFINEMENT_NONE ||
            in->special_confinement[i] > GAME_CONFINEMENT_HOSPITAL) return 0;
    next=*state; next.prison_node=in->prison_node; next.hospital_node=in->hospital_node;
    for (i=0;i<GAME_MAX_PLAYERS;i++) restore_player(&next.players[i],&in->players[i]);
    memcpy(next.special_player_node,in->special_node,sizeof(in->special_node));
    memcpy(next.special_player_actor_type,in->special_actor_type,sizeof(in->special_actor_type));
    memcpy(next.special_player_confinement,in->special_confinement,sizeof(in->special_confinement));
    *state=next;
    return 1;
}
