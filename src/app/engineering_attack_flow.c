#include "app/engineering_attack_flow.h"

#include <limits.h>
#include <string.h>

#include "view/game_view_model.h"

int rich4_engineering_attack_begin(GameSession *session, int player,
                                    int node, uint32_t now)
{
    GameEngineeringAttackState *attack;
    const GamePlayerState *p;
    GameState *s;
    int raw, id, facility, owner, level;
    if (session == NULL) return 0;
    s = &session->state;
    attack = &session->presentation.engineering_attack;
    if (attack->active || player < 0 || player >= s->player_count ||
        node <= 0 || node >= s->tool_route_node_count) return 0;
    p = &s->players[player];
    /* 0x448AD1: traffic +17, NOT AI personality +23. */
    if (!(p->play_state & 0xff) || (p->traffic_method & 0x83) != 3 ||
        (p->days_sleep_walking & 0xff) ||
        (p->delayed_status_block & 0xff)) return 0;
    raw = s->route_object_raw[node];
    facility = raw > 4000 && raw < 6000;
    if (!facility && !(raw > 2000 && raw < 4000)) return 0;
    id = raw - (facility ? 4001 : 2001);
    if (id < 0 || id >= (facility ? s->facility_count : s->land_count))
        return 0;
    owner = facility ? s->facility_owner[id] : s->land_owner[id];
    level = facility ? s->facility_level[id] : s->land_level[id];
    if (owner == player || level == 0) return 0;
    memset(attack, 0, sizeof(*attack));
    attack->active = 1;
    attack->player = player;
    attack->node = node;
    attack->property = id;
    attack->facility = facility;
    attack->effect_frame = -1;
    attack->world_x = facility ? s->tool_facility_x[id] : s->tool_land_x[id];
    attack->world_y = facility ? s->tool_facility_y[id] : s->tool_land_y[id];
    attack->tick = session->presentation.movement_tick != 0
        ? session->presentation.movement_tick : now;
    /* 0x41D546 clears the alternate camera target before entering EA2=3. */
    session->presentation.has_view_override = 0;
    rich4_presentation_transition(&session->presentation,
                                  RICH4_PRESENTATION_TOOL_ANIMATION, NULL);
    return 1;
}

static void attack_relation(GameState *s, int owner, int player)
{
    int64_t value, delta = (int64_t)30 * s->price_index;
    if (owner < 0 || owner >= s->player_count || owner == player) return;
    value = (int64_t)s->players[owner].relation_balance[player] + delta;
    s->players[owner].relation_balance[player] =
        value < 0 ? 0 : value > INT_MAX ? INT_MAX : (int)value;
    /* 0x40DF69 -> 0x40CC1A clears both +61/+65, not god attachment +63. */
    if (delta > 0 && s->players[owner].linked_player == player) {
        s->players[owner].linked_player = -1;
        s->players[owner].god_status_days = 0;
        s->players[player].linked_player = -1;
        s->players[player].god_status_days = 0;
    }
}

static void demolish_building(GameState *s, const GameEngineeringAttackState *a)
{
    int id = a->property;
    int owner = a->facility ? s->facility_owner[id] : s->land_owner[id];
    int *level = a->facility ? &s->facility_level[id] : &s->land_level[id];
    int *type = a->facility ? &s->facility_type[id] : &s->land_type[id];
    int *value = a->facility ? &s->facility_value[id] : &s->land_value[id];
    int price = a->facility ? s->facility_price[id] : s->land_price[id];
    int i;
    if (*level == 0) return;
    /* 0x40AB4A(raw,2): retain owner, variant and +48/+52 runtime fields. */
    *level = 0;
    *type = 0;
    /* These two SDL values cache assets derived from the property record. */
    if (owner >= 0 && owner < s->player_count) {
        int64_t total = (int64_t)s->fixed_asset_value[owner] + price - *value;
        s->fixed_asset_value[owner] =
            total < 0 ? 0 : total > INT_MAX ? INT_MAX : (int)total;
    }
    *value = price;
    if (!a->facility) return;
    /* 0x40DFFA does not filter by the demolished facility's occupants. */
    for (i = 0; i < s->player_count; ++i)
        if ((s->players[i].play_state & 0xff) &&
            (s->players[i].delayed_status_block & 0xff))
            s->players[i].delayed_status_block = 0x80;
}

int rich4_engineering_attack_update(GameSession *session, int actor_frames,
                                     int effect_frames, int tick_ms,
                                     uint32_t now,
                                     Rich4AudioEventQueue *audio_events)
{
    GameEngineeringAttackState *a;
    GameActorPresentationState *actor;
    GameState *s;
    int frame;
    if (session == NULL || !session->presentation.engineering_attack.active)
        return 0;
    a = &session->presentation.engineering_attack;
    /* Missing original assets must not turn into an invented timer/animation. */
    if (actor_frames < 4 || effect_frames <= 0 || tick_ms <= 0) return 1;
    if (now - a->tick < (uint32_t)tick_ms) return 1;
    /* 0x401F98 supplies one pending pulse, never a catch-up frame loop. */
    a->tick += ((now - a->tick) / (uint32_t)tick_ms) * (uint32_t)tick_ms;
    session->presentation.movement_tick = a->tick;
    session->flow.property_turn_end_tick = a->tick;
    s = &session->state;
    actor = &session->presentation.actors[a->player];
    if (!a->initialized) {
        attack_relation(s, a->facility ? s->facility_owner[a->property]
                                      : s->land_owner[a->property], a->player);
        a->saved_direction = actor->direction;
        actor->direction = rich4_game_actor_direction_from_delta(
            a->world_x - s->tool_route_x[a->node],
            a->world_y - s->tool_route_y[a->node]);
        a->initialized = 1;
        a->hold_ticks = 6;
        a->actor_frame = 0;
        a->cursor = 1; /* 0x40DD08 increments AFTER drawing frame zero. */
        return 1;
    }
    if (a->hold_ticks > 0) {
        --a->hold_ticks;
        return 1;
    }
    frame = a->cursor;
    if (frame == 3) demolish_building(s, a);
    if (frame >= 2) {
        if (frame == 2)
            rich4_audio_event_emit(audio_events, RICH4_AUDIO_EVENT_TOOL_EFFECT,
                                    1, 15);
        if (a->effect_frame + 1 < effect_frames) ++a->effect_frame;
        /* 0x450F04 redraws the actor before 0x40DC5E increments EA3. */
        a->actor_frame = frame;
    }
    if (a->cursor < actor_frames - 1) ++a->cursor;
    if (frame < 2) a->actor_frame = a->cursor;
    if (a->cursor == actor_frames - 1 &&
        a->effect_frame == effect_frames - 1) {
        actor->direction = a->saved_direction;
        actor->walk_frame = 0;
        a->active = 0;
        {
            Rich4PresentationActivity activity =
                rich4_presentation_activity_from_state(&session->presentation, s);
            rich4_presentation_transition(&session->presentation,
                rich4_presentation_mode_resolve(&activity), NULL);
        }
    }
    /* Even the finishing pulse belongs to EA2=3: don't decrement EA5 yet. */
    return 1;
}
