#include "core/ai.h"
#include "core/ai_internal.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "core/facility.h"
#include "core/god.h"
#include "core/property.h"
#include "core/stock.h"
#include "core/tool_internal.h"
#include "data/rich4_map_tables.inc"

static GameDomainRef none_ref(void)
{
    return rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
}

static int ai_collect_visible_property_refs(const GameState *s, int p,
                                            GameDomainRef *out, int capacity);
static int ai_property_geometry_present(const GameState *s);

static void trace(GameAiDecision *d, GameDomainRef subject,
                  GameDomainRef target, GameAiCandidateResult result,
                  int score, int value)
{
    GameAiCandidateTrace *c;
    if (d->candidate_count >= GAME_AI_TRACE_CAPACITY) {
        d->candidate_overflow = 1;
        return;
    }
    c = &d->candidates[d->candidate_count++];
    c->subject = subject;
    c->target = target;
    c->result = result;
    c->score = score;
    c->value = value;
}

static int active_ai(const GameState *s, int p)
{
    return s != NULL && p >= 0 && p < s->player_count &&
           s->players[p].is_ai &&
           (s->players[p].play_state & GAME_PLAYER_PLAY_STATE_MASK) != 0;
}

int rich4_ai_purchase_allowed(const GameState *state, int actor, int price)
{
    int64_t reserve;
    int64_t funds;
    if (state == NULL || actor < 0 || actor >= state->player_count ||
        state->initial_fund < 0 || state->price_index <= 0)
        return 0;
    /* 0x41D7E6 -> 0x457DBC truncates F * 0.05 toward zero. */
    reserve = (int64_t)((double)state->initial_fund * 0.05);
    funds = (int64_t)state->players[actor].cash + state->players[actor].bank;
    if (reserve > 7000) reserve = 7000;
    reserve *= state->price_index;
    return funds - price > reserve;
}

static int personality_gate(int difficulty, int personality, Rich4Rng *rng,
                            GameAiCandidateResult *reason)
{
    int difference = difficulty - personality;
    if (difference >= 2) {
        *reason = GAME_AI_CANDIDATE_DIFFICULTY;
        return 0;
    }
    if (difference == 1 && rich4_rng_original_state_range(rng, 3) != 0) {
        *reason = GAME_AI_CANDIDATE_RANDOM_GATE;
        return 0;
    }
    return 1;
}

static int relation_preferred_player(const GameState *s, int p)
{
    int best = -1, best_value = 0, i;
    for (i = 0; i < s->player_count; i++) {
        int value;
        if (i == p || (s->players[i].play_state &
                       GAME_PLAYER_PLAY_STATE_MASK) == 0) continue;
        value = s->players[p].relation_balance[i];
        if (value > best_value) {
            best = i;
            best_value = value;
        }
    }
    return best;
}

/* App callers can provide a precomputed visibility array, but the core also
 * has enough route geometry to derive the ordinary active-player subset for
 * direct AI selection.  Keep the explicit array authoritative when present;
 * this fallback only activates when no app context was supplied. */
static int ai_project_player_map(const GameState *s, uint8_t *out)
{
    int saw_player_mask = 0;
    int i;
    int node;
    int found = 0;
    if (s == NULL || out == NULL || s->tool_route_node_count <= 1)
        return 0;
    memset(out, 0, GAME_MAX_PLAYERS * sizeof(*out));

    /* The original visible-object table encodes ordinary players in the
     * low nibble of a high-bit object.  When that projection is available,
     * it is authoritative: do not widen visibility merely because another
     * player happens to have a valid route node. */
    for (node = 1; node < s->tool_route_node_count; node++) {
        uint16_t raw = s->route_object_raw[node];
        if ((raw & 0x8000u) == 0 || (raw & 0x000fu) == 0) continue;
        saw_player_mask = 1;
        for (i = 0; i < s->player_count && i < GAME_MAX_PLAYERS &&
                    i < 4; i++) {
            if ((raw & (uint16_t)(1u << i)) != 0 &&
                rich4_tool_active_player(s, i)) {
                out[i] = 1;
                found = 1;
            }
        }
    }
    /* Presence of any high-bit mask is itself authoritative.  The original
     * global object table can contain a mask whose bits do not correspond to
     * a currently active player; that still means the table was available,
     * so callers must not widen the result with typed route-node fallback. */
    if (saw_player_mask) return 1;

    /* Synthetic/unit states often carry route geometry but no player-object
     * records.  Retain the previous typed fallback for those states. */
    for (i = 0; i < s->player_count && i < GAME_MAX_PLAYERS; i++) {
        if (!rich4_tool_active_player(s, i) ||
            !rich4_tool_valid_node(s, s->players[i].node)) continue;
        out[i] = 1;
        found = 1;
    }
    return found;
}

static GameDomainRef ai_project_current_property(const GameState *s, int p)
{
    uint16_t raw;
    int node;
    int i;
    if (s == NULL || p < 0 || p >= s->player_count ||
        s->tool_route_node_count <= 1 ||
        !rich4_tool_valid_node(s, s->players[p].node))
        return none_ref();
    node = s->players[p].node;
    raw = s->route_object_raw[node];
    if (raw > 0x7d0 && raw < 0xfa0) {
        i = (int)raw - 0x7d1;
        if (i >= 0 && i < s->land_count)
            return rich4_game_domain_ref(GAME_DOMAIN_LAND, i);
    }
    if (raw > 0xfa0 && raw < 0x1770) {
        i = (int)raw - 0xfa1;
        if (i >= 0 && i < s->facility_count)
            return rich4_game_domain_ref(GAME_DOMAIN_FACILITY, i);
    }
    for (i = 0; i < s->land_count; i++)
        if (s->tool_land_x[i] == s->tool_route_x[node] &&
            s->tool_land_y[i] == s->tool_route_y[node])
            return rich4_game_domain_ref(GAME_DOMAIN_LAND, i);
    for (i = 0; i < s->facility_count; i++)
        if (s->tool_facility_x[i] == s->tool_route_x[node] &&
            s->tool_facility_y[i] == s->tool_route_y[node])
            return rich4_game_domain_ref(GAME_DOMAIN_FACILITY, i);
    return none_ref();
}

/* Normalize the typed object subset available in GameState: properties,
 * auxiliary/company records, actors, route nodes, and placed tool objects.
 * This is only a candidate projection; it does not reproduce the distinct
 * original sources of 0x48B8C4.  0x409EF9 projects flagged route records,
 * while 0x40A45C first consumes the runtime object pool through 0x409DE7.
 * Unknown raw IDs remain present with a NONE reference so callers can retain
 * object-existence gates without inventing a target mapping. */
typedef struct AiProjectedObject {
    uint16_t raw;
    int node;
    int x;
    int y;
    int projected_x;
    int projected_y;
    GameDomainRef ref;
} AiProjectedObject;

#define AI_PROJECTED_OBJECT_CAPACITY \
    (GAME_MAX_MAP_ROUTE_NODES * 8 + GAME_MAX_MAP_PROPERTIES + \
     GAME_MAX_MAP_PROPERTIES + GAME_SPECIAL_PLAYER_COUNT + \
     GAME_TOOL_PLACED_OBJECT_COUNT)

static int ai_projected_object_append(AiProjectedObject *out, int capacity,
                                      int count, uint16_t raw, int node,
                                      int x, int y, GameDomainRef ref)
{
    if (count >= capacity) return count;
    out[count].raw = raw;
    out[count].node = node;
    out[count].x = x;
    out[count].y = y;
    out[count].projected_x = x;
    out[count].projected_y = y;
    out[count].ref = ref;
    return count + 1;
}

/* Match the row-major order used by original 440x440 scans for the projected
 * subset.  The original route table (0x409EF9) and runtime-object table
 * (0x40A45C/0x409DE7) have different sources and overwrite/OR rules; a stable
 * tie here preserves only this collector's order. */
static int ai_projected_object_before_global(const AiProjectedObject *left,
                                             const AiProjectedObject *right)
{
    if (left->projected_y != right->projected_y)
        return left->projected_y < right->projected_y;
    return left->projected_x < right->projected_x;
}

static void ai_projection_subtile_offset(int direction, int world_x,
                                         int world_y, int *out_x, int *out_y)
{
    static const signed char coefficients[8][4] = {
        {-34, 11, -14, -25}, {-34, -9, 16, -25}, {-13, -25, 37, -9},
        {16, -25, 36, 11}, {37, -9, 16, 26}, {36, 11, -14, 26},
        {16, 26, -35, 11}, {-14, 26, -35, -9}
    };
    const signed char *c = coefficients[direction & 7];
    int sub_x = world_x & 31;
    int sub_y = world_y & 31;
    if (out_x != NULL)
        *out_x = ((sub_x * c[0]) >> 5) + ((sub_y * c[2]) >> 5);
    if (out_y != NULL)
        *out_y = ((sub_x * c[1]) >> 5) + ((sub_y * c[3]) >> 5);
}

static int ai_global_projection_screen_xy(
    const GameAiGlobalProjection *projection, int world_x, int world_y,
    int *out_x, int *out_y)
{
    int tile_x;
    int tile_y;
    int center_sub_x;
    int center_sub_y;
    int point_sub_x;
    int point_sub_y;
    int screen_x;
    int screen_y;

    if (projection == NULL || out_x == NULL || out_y == NULL) return 0;
    tile_x = (world_x >> 5) - (projection->center_world_x >> 5) + 14;
    tile_y = (world_y >> 5) - (projection->center_world_y >> 5) + 14;
    if (tile_x < 0 || tile_x > 28 || tile_y < 0 || tile_y > 28)
        return 0;
    ai_projection_subtile_offset(projection->direction,
                                 projection->center_world_x,
                                 projection->center_world_y,
                                 &center_sub_x, &center_sub_y);
    ai_projection_subtile_offset(projection->direction, world_x, world_y,
                                 &point_sub_x, &point_sub_y);
    screen_x = 220 + center_sub_x +
        rich4_map_grid[projection->direction & 7][tile_y][tile_x][1] -
        point_sub_x;
    screen_y = 220 + center_sub_y +
        rich4_map_grid[projection->direction & 7][tile_y][tile_x][0] -
        point_sub_y;
    if (screen_x < 0 || screen_x >= 440 ||
        screen_y < 0 || screen_y >= 440)
        return 0;
    *out_x = screen_x;
    *out_y = screen_y;
    return 1;
}

/* 0x409EF9/0x42114F read live occupancy in node +36. SDL's route flags
 * retain map data, while moving actors and placed tools live in typed state. */
static int ai_route_has_occupant(const GameState *s,
                                const GameAiGlobalProjection *projection,
                                int node, uint32_t mask)
{
    int i;
    if ((s->tool_route_flags[node] & mask) != 0) return 1;
    if (projection != NULL && projection->occupied_dynamic_nodes != NULL)
        for (i = 0; i < projection->occupied_dynamic_node_count; i++)
            if (projection->occupied_dynamic_nodes[i] == node) return 1;
    for (i = 0; i < s->player_count && i < 4; i++) {
        const GamePlayerState *p = &s->players[i];
        if ((p->play_state & 0xff) != 0 && p->node == node &&
            !p->delayed_status_block &&
            !p->days_disappearing && !p->days_in_prison &&
            !p->days_in_hospital)
            return 1;
    }
    for (i = 0; i < GAME_SPECIAL_PLAYER_COUNT; i++)
        if (s->special_player_node[i] == node &&
            s->special_player_f10[i] == 0)
            return 1;
    return rich4_tool_object_on_node(s, node) >= 0;
}

/* 0x409EF9 builds the independent road table; later route records overwrite
 * earlier records at the same pixel, then it compacts in row-major order. */
static int ai_collect_global_route_nodes(
    const GameState *s, const GameAiGlobalProjection *projection,
    int *out_nodes, int capacity)
{
    uint16_t *screen_cells;
    int count = 0;
    int node;
    int y;

    if (s == NULL || projection == NULL || out_nodes == NULL || capacity <= 0 ||
        s->tool_route_node_count <= 1 ||
        s->tool_route_node_count > GAME_MAX_MAP_ROUTE_NODES)
        return -1;
    screen_cells = (uint16_t *)calloc(440u * 440u, sizeof(*screen_cells));
    if (screen_cells == NULL) return -1;
    for (node = 1; node < s->tool_route_node_count; node++) {
        int x;
        int screen_x;
        int screen_y;
        if (ai_route_has_occupant(s, projection, node, 0x00ffff00u) ||
            !ai_global_projection_screen_xy(
                projection, s->tool_route_x[node], s->tool_route_y[node],
                &screen_x, &screen_y))
            continue;
        x = screen_y * 440 + screen_x;
        screen_cells[x] = (uint16_t)node;
    }
    for (y = 0; y < 440; y++) {
        int x;
        for (x = 0; x < 440; x++) {
            uint16_t candidate = screen_cells[y * 440 + x];
            if (candidate == 0) continue;
            if (count >= capacity) {
                free(screen_cells);
                return -1;
            }
            out_nodes[count++] = candidate;
        }
    }
    free(screen_cells);
    return count;
}

/* Project the typed subset into the original 440x440 coordinate frame, then
 * sort it in row-major pixel order. This does not claim the source filters,
 * raw-object composition, or cell collision behavior of either 0x409EF9 or
 * 0x40A45C(-1). Return the compacted visible-object count. */
static int ai_prepare_global_objects(
    const GameState *s, AiProjectedObject *objects, int count, int capacity,
    const GameAiGlobalProjection *projection)
{
    int i;

    /* 0x40865B queues ordinary actors before map objects. Preserve that
     * source order before the row-major sort; same-pixel winner selection is
     * still not represented by this typed projection. */
    if (s != NULL && projection != NULL && objects != NULL &&
        capacity > count) {
        int actor_count = 0;
        int actor;
        for (actor = 0; actor < s->player_count &&
                        actor < GAME_MAX_PLAYERS; actor++)
            if ((projection->visible_player_mask & (1u << actor)) != 0)
                actor_count++;
        if (actor_count > capacity - count)
            actor_count = capacity - count;
        if (actor_count > 0) {
            memmove(objects + actor_count, objects,
                    (size_t)count * sizeof(*objects));
            {
                int inserted = 0;
                for (actor = 0; actor < s->player_count &&
                                actor < GAME_MAX_PLAYERS &&
                                inserted < actor_count; actor++) {
                    AiProjectedObject *object;
                    int raw;
                    if ((projection->visible_player_mask & (1u << actor)) == 0)
                        continue;
                    raw = (int)(0x8000u | (1u << actor));
                    object = &objects[inserted++];
                    object->raw = (uint16_t)raw;
                    object->node = s->players[actor].node;
                    object->x = projection->player_world_x[actor];
                    object->y = projection->player_world_y[actor];
                    object->projected_x = object->x;
                    object->projected_y = object->y;
                    object->ref = rich4_game_domain_ref(
                        GAME_DOMAIN_PLAYER, actor);
                }
                count += inserted;
            }
        }
    }
    {
        int kept = 0;
        for (i = 0; i < count; i++) {
            AiProjectedObject item = objects[i];
            if (projection != NULL) {
                if (!ai_global_projection_screen_xy(
                        projection, item.x, item.y,
                        &item.projected_x, &item.projected_y)) continue;
            }
            objects[kept++] = item;
        }
        count = kept;
    }
    for (i = 1; i < count; i++) {
        AiProjectedObject item = objects[i];
        int pos = i;
        while (pos > 0 && ai_projected_object_before_global(
                   &item, &objects[pos - 1])) {
            objects[pos] = objects[pos - 1];
            pos--;
        }
        objects[pos] = item;
    }
    return count;
}

/* The global candidate scans in 0x421717 and 0x420970 consume ordinary
 * player bits from the runtime object table built by 0x409DE7/0x40A45C. An
 * SDL caller may supply that table's raw IDs after pixel-cell OR; core-only
 * projections retain the actor-mask fallback. */
static int ai_prepare_global_player_masks(
    const GameState *s, AiProjectedObject *objects, int capacity,
    const GameAiGlobalProjection *projection)
{
    int count = 0;
    int actor;
    int i;

    if (s == NULL || objects == NULL || capacity <= 0 ||
        projection == NULL)
        return 0;
    if (projection->object_table_available) {
        int kept;
        if (projection->object_count < 0 ||
            (projection->object_count > 0 && projection->objects == NULL) ||
            projection->object_count > capacity)
            return -1;
        for (i = 0; i < projection->object_count; i++) {
            const GameAiGlobalObjectProjection *source =
                &projection->objects[i];
            if (source->raw_id == 0 || source->screen_x < 0 ||
                source->screen_x >= 440 || source->screen_y < 0 ||
                source->screen_y >= 440)
                continue;
            count = ai_projected_object_append(
                objects, capacity, count, source->raw_id, 0,
                source->screen_x, source->screen_y, none_ref());
        }
        for (i = 1; i < count; i++) {
            AiProjectedObject item = objects[i];
            int pos = i;
            while (pos > 0 && ai_projected_object_before_global(
                       &item, &objects[pos - 1])) {
                objects[pos] = objects[pos - 1];
                pos--;
            }
            objects[pos] = item;
        }
        /* 0x409DE7 ORs every eligible raw ID into its pixel before
         * 0x40A45C compacts the 440x440 buffer in row-major order. */
        kept = 0;
        for (i = 0; i < count; i++) {
            if (kept > 0 &&
                objects[kept - 1].projected_x == objects[i].projected_x &&
                objects[kept - 1].projected_y == objects[i].projected_y) {
                objects[kept - 1].raw |= objects[i].raw;
                continue;
            }
            if (kept != i) objects[kept] = objects[i];
            kept++;
        }
    return kept;
}

    for (actor = 0; actor < s->player_count && actor < GAME_MAX_PLAYERS &&
                    actor < 4 && count < capacity; actor++) {
        if ((projection->visible_player_mask & (1u << actor)) == 0 ||
            (projection->positioned_player_mask & (1u << actor)) == 0)
            continue;
        count = ai_projected_object_append(
            objects, capacity, count,
            (uint16_t)(0x8000u | (1u << actor)),
            s->players[actor].node,
            projection->player_world_x[actor],
            projection->player_world_y[actor],
            rich4_game_domain_ref(GAME_DOMAIN_PLAYER, actor));
    }
    {
        int kept = 0;
        for (i = 0; i < count; i++) {
            AiProjectedObject item = objects[i];
            if (!ai_global_projection_screen_xy(
                    projection, item.x, item.y,
                    &item.projected_x, &item.projected_y))
                continue;
            objects[kept++] = item;
        }
        count = kept;
    }
    for (i = 1; i < count; i++) {
        AiProjectedObject item = objects[i];
        int pos = i;
        while (pos > 0 && ai_projected_object_before_global(
                   &item, &objects[pos - 1])) {
            objects[pos] = objects[pos - 1];
            pos--;
        }
        objects[pos] = item;
    }
    return count;
}

/* 0x41EAE2, 0x41F037, 0x41F1B3, 0x41F400, and 0x42040E scan the full
 * 0x40A45C(-1) raw table. Preserve its pixel OR and row-major order before
 * decoding property IDs; callers apply each original handler's filters.
 * Core-only states keep the established typed viewport fallback. */
static int ai_collect_card_global_property_refs(
    const GameState *s, int p, const GameAiGlobalProjection *projection,
    GameDomainRef *out, int capacity)
{
    AiProjectedObject objects[AI_PROJECTED_OBJECT_CAPACITY];
    int object_count;
    int count = 0;
    int i;

    if (projection == NULL || !projection->object_table_available)
        return ai_collect_visible_property_refs(s, p, out, capacity);
    if (s == NULL || out == NULL || capacity <= 0) return 0;
    object_count = ai_prepare_global_player_masks(
        s, objects, AI_PROJECTED_OBJECT_CAPACITY, projection);
    if (object_count < 0) return 0;
    for (i = 0; i < object_count; i++) {
        uint16_t raw = objects[i].raw;
        int id;
        if (raw > 0x07d0u && raw < 0x0fa0u) {
            id = (int)raw - 0x07d1;
            if (id < 0 || id >= s->land_count) continue;
            if (count >= capacity) return 0;
            out[count++] = rich4_game_domain_ref(GAME_DOMAIN_LAND, id);
        } else if (raw > 0x0fa0u && raw < 0x1770u) {
            id = (int)raw - 0x0fa1;
            if (id < 0 || id >= s->facility_count) continue;
            if (count >= capacity) return 0;
            out[count++] = rich4_game_domain_ref(
                GAME_DOMAIN_FACILITY, id);
        }
    }
    return count;
}

/* The ordinary-player masks in 0x48B8C4 are the visibility source for the
 * original card handlers at 0x41E779, 0x41F901, 0x41FACC, 0x4202D2, and
 * 0x4207CC. When SDL has supplied the raw table, do not widen that set from
 * route nodes alone. */
static int ai_project_visible_player_mask(
    const GameState *s, const GameAiGlobalProjection *projection,
    uint8_t out_on_map[GAME_MAX_PLAYERS])
{
    AiProjectedObject objects[AI_PROJECTED_OBJECT_CAPACITY];
    int count;
    int i;
    int player;

    if (s == NULL || projection == NULL || out_on_map == NULL ||
        !projection->object_table_available)
        return 0;
    memset(out_on_map, 0, GAME_MAX_PLAYERS * sizeof(out_on_map[0]));
    count = ai_prepare_global_player_masks(
        s, objects, AI_PROJECTED_OBJECT_CAPACITY, projection);
    if (count < 0) return -1;
    for (i = 0; i < count; i++) {
        uint16_t raw = objects[i].raw;
        if ((raw & 0x8000u) == 0 || (raw & 0x000fu) == 0) continue;
        for (player = 0; player < s->player_count &&
                         player < GAME_MAX_PLAYERS && player < 4; player++)
            if ((raw & (uint16_t)(1u << player)) != 0)
                out_on_map[player] = 1;
    }
    for (player = 0; player < s->player_count &&
                     player < GAME_MAX_PLAYERS; player++)
        if ((s->players[player].play_state &
             GAME_PLAYER_PLAY_STATE_MASK) == 0 ||
            s->players[player].days_disappearing != 0)
            out_on_map[player] = 0;
    return 1;
}

static int ai_collect_visible_player_order(
    const GameState *s, const GameAiGlobalProjection *projection,
    uint8_t out_order[GAME_MAX_PLAYERS], int *out_count)
{
    AiProjectedObject objects[AI_PROJECTED_OBJECT_CAPACITY];
    unsigned int seen = 0;
    int count;
    int i;
    int player;

    if (out_count != NULL) *out_count = 0;
    if (s == NULL || projection == NULL || out_order == NULL ||
        out_count == NULL || !projection->object_table_available)
        return 0;
    count = ai_prepare_global_player_masks(
        s, objects, AI_PROJECTED_OBJECT_CAPACITY, projection);
    if (count < 0) return -1;
    for (i = 0; i < count; i++) {
        uint16_t raw = objects[i].raw;
        if ((raw & 0x8000u) == 0 || (raw & 0x000fu) == 0) continue;
        for (player = 0; player < s->player_count &&
                         player < GAME_MAX_PLAYERS && player < 4; player++) {
            unsigned int bit = 1u << player;
            if ((raw & (uint16_t)bit) == 0 || (seen & bit) != 0 ||
                (s->players[player].play_state &
                 GAME_PLAYER_PLAY_STATE_MASK) == 0 ||
                s->players[player].days_disappearing != 0)
                continue;
            seen |= bit;
            out_order[(*out_count)++] = (uint8_t)player;
        }
    }
    return 1;
}

static int ai_collect_projected_objects(const GameState *s,
                                        AiProjectedObject *out,
                                        int capacity)
{
    int count = 0;
    int node;
    int slot;

    if (s == NULL || out == NULL || capacity <= 0 ||
        s->tool_route_node_count <= 1)
        return 0;
    for (node = 1; node < s->tool_route_node_count; node++) {
        uint16_t raw = s->route_object_raw[node];
        int x;
        int y;
        int bit;

        if (raw == 0) continue;
        x = s->tool_route_x[node];
        y = s->tool_route_y[node];
        if ((raw & 0x8000u) != 0) {
            int added = 0;
            /* Bits 0..3 are the ordinary-player mask used by the original
             * AI target scans; bits 4..7 are the special-player mask. */
            for (bit = 0; bit < 4; bit++) {
                if ((raw & (uint16_t)(1u << bit)) != 0) {
                    count = ai_projected_object_append(
                        out, capacity, count, raw, node, x, y,
                        rich4_game_domain_ref(GAME_DOMAIN_PLAYER, bit));
                    added = 1;
                }
            }
            for (bit = 0; bit < 4; bit++) {
                if ((raw & (uint16_t)(1u << (bit + 4))) != 0) {
                    count = ai_projected_object_append(
                        out, capacity, count, raw, node, x, y,
                        rich4_game_domain_ref(GAME_DOMAIN_SPECIAL_PLAYER,
                                              bit));
                    added = 1;
                }
            }
            if (!added && (raw & 0x7f00u) != 0) {
                count = ai_projected_object_append(
                    out, capacity, count, raw, node, x, y, none_ref());
            }
            continue;
        }

        {
            GameDomainRef ref = none_ref();
            if (raw > 0x7d0u && raw < 0xfa0u) {
                ref = rich4_game_domain_ref(GAME_DOMAIN_LAND,
                                             (int)raw - 0x7d1);
            } else if (raw > 0xfa0u && raw < 0x1770u) {
                ref = rich4_game_domain_ref(GAME_DOMAIN_FACILITY,
                                             (int)raw - 0xfa1);
            } else if (raw == 0x1770u) {
                /* 6000 is the zero-based stock object; 6001 onward is the
                 * one-based auxiliary/company range used by the original AI. */
                ref = rich4_game_domain_ref(GAME_DOMAIN_STOCK, 0);
            } else if (raw > 0x1770u && raw < 0x1f40u) {
                ref = rich4_game_domain_ref(GAME_DOMAIN_COMPANY,
                                             (int)raw - 0x1771);
            } else if (raw < 0x7d1u) {
                /* Road/static route objects are addressable by their route
                 * node in the current core, even when their raw subtype is
                 * not otherwise decoded. */
                ref = rich4_game_domain_ref(GAME_DOMAIN_ROUTE_NODE, node);
            }
            count = ai_projected_object_append(
                out, capacity, count, raw, node, x, y, ref);
        }
    }

    /* Real maps supply authoritative property geometry separately from the
     * route-node raw table.  The original global renderer table still has a
     * visible entry for each built property even when its route node does
     * not repeat the typed raw ID. */
    if (ai_property_geometry_present(s)) {
        for (slot = 0; slot < s->land_count &&
                       slot < GAME_MAX_MAP_PROPERTIES; slot++) {
            uint16_t raw = (uint16_t)(0x7d1 + slot);
            int exists = 0;
            int j;
            if (s->land_owner[slot] < 0 && s->land_level[slot] <= 0)
                continue;
            for (j = 0; j < count; j++)
                if (out[j].raw == raw) { exists = 1; break; }
            if (!exists)
                count = ai_projected_object_append(
                    out, capacity, count, raw, -1,
                    s->tool_land_x[slot], s->tool_land_y[slot],
                    rich4_game_domain_ref(GAME_DOMAIN_LAND, slot));
        }
        for (slot = 0; slot < s->facility_count &&
                       slot < GAME_MAX_MAP_PROPERTIES; slot++) {
            uint16_t raw = (uint16_t)(0xfa1 + slot);
            int exists = 0;
            int j;
            if (s->facility_owner[slot] < 0 &&
                s->facility_level[slot] <= 0)
                continue;
            for (j = 0; j < count; j++)
                if (out[j].raw == raw) { exists = 1; break; }
            if (!exists)
                count = ai_projected_object_append(
                    out, capacity, count, raw, -1,
                    s->tool_facility_x[slot], s->tool_facility_y[slot],
                    rich4_game_domain_ref(GAME_DOMAIN_FACILITY, slot));
        }
    }

    /* Moving special actors are not duplicated in the immutable route-object
     * array.  The original 0x48B8C4 table still exposes their high-bit actor
     * records at the current route node, so add the typed state equivalent
     * for object-existence gates.  Keep the special-player ref explicit: the
     * ordinary-player candidate scans below only consume low-nibble bits. */
    for (slot = 0; slot < GAME_SPECIAL_PLAYER_COUNT; slot++) {
        int special_node = s->special_player_node[slot];
        uint16_t raw;
        if (s->special_player_f10[slot] != 0 ||
            !rich4_tool_valid_node(s, special_node))
            continue;
        raw = (uint16_t)(0x8000u | (1u << (slot + 4)));
        count = ai_projected_object_append(
            out, capacity, count, raw, special_node,
            s->tool_route_x[special_node], s->tool_route_y[special_node],
            rich4_game_domain_ref(GAME_DOMAIN_SPECIAL_PLAYER, slot));
    }

    /* Auxiliary/company records are part of the original global object
     * table, but are not guaranteed to have a duplicate route raw ID.  The
     * map loader supplies their coordinates explicitly so area-object scans
     * do not silently miss company-only map objects. */
    if (s->tool_company_geometry_available) {
        for (slot = 0; slot < s->company_count &&
                    slot < GAME_MAX_MAP_PROPERTIES; slot++) {
            count = ai_projected_object_append(
                out, capacity, count, (uint16_t)(0x1771 + slot), -1,
                s->tool_company_x[slot], s->tool_company_y[slot],
                rich4_game_domain_ref(GAME_DOMAIN_COMPANY, slot));
        }
    }

    for (slot = 0; slot < GAME_TOOL_PLACED_OBJECT_COUNT; slot++) {
        const struct GameToolPlacedObjectState *object =
            &s->tool_placed_objects[slot];
        if (object->kind == 0 || !rich4_tool_valid_node(s, object->node))
            continue;
        count = ai_projected_object_append(
            out, capacity, count, 0, object->node,
            s->tool_route_x[object->node], s->tool_route_y[object->node],
            rich4_game_domain_ref(GAME_DOMAIN_MAP_OBJECT, slot));
    }
    return count;
}

/* A zero coordinate is valid on a real map.  Keep the existing all-zero
 * convention only for synthetic states that have not supplied any property
 * geometry at all. */
static int ai_property_geometry_present(const GameState *s)
{
    int i;
    if (s == NULL) return 0;
    for (i = 0; i < s->land_count && i < GAME_MAX_MAP_PROPERTIES; i++)
        if (s->tool_land_x[i] != 0 || s->tool_land_y[i] != 0) return 1;
    for (i = 0; i < s->facility_count && i < GAME_MAX_MAP_PROPERTIES; i++)
        if (s->tool_facility_x[i] != 0 || s->tool_facility_y[i] != 0)
            return 1;
    return 0;
}

/* The original 0x40A45C(-1) path rebuilds the full 440x440 object table,
 * then 0x421BA6 scans that table row-major. When the SDL caller supplies the
 * visible raw table, its pixel order and same-cell OR are authoritative;
 * geometry supplies a reduced static-property fallback for core-only states.
 * Return -1 when neither source can establish property coordinates. */
typedef struct AiGlobalProperty {
    GameDomainRef ref;
    int order_x;
    int order_y;
} AiGlobalProperty;

static int ai_global_property_before(const AiGlobalProperty *left,
                                     const AiGlobalProperty *right)
{
    if (left->order_y != right->order_y)
        return left->order_y < right->order_y;
    if (left->order_x != right->order_x)
        return left->order_x < right->order_x;
    /* The typed fallback appends lands before facilities at an equal tile. */
    if (left->ref.kind != right->ref.kind)
        return left->ref.kind == GAME_DOMAIN_LAND;
    return left->ref.id < right->ref.id;
}

static int ai_collect_global_properties(const GameState *s,
                                        const GameAiGlobalProjection *projection,
                                        AiGlobalProperty *out,
                                        int capacity)
{
    int count = 0;
    int i;

    if (s == NULL || out == NULL || capacity <= 0)
        return -1;
    if (projection != NULL && projection->object_table_available) {
        AiProjectedObject objects[AI_PROJECTED_OBJECT_CAPACITY];
        int object_count = ai_prepare_global_player_masks(
            s, objects, AI_PROJECTED_OBJECT_CAPACITY, projection);
        if (object_count < 0) return -1;
        /* 0x421BA6 consumes 0x40A45C(-1) in screen row order. Decode only
         * the ordinary land/facility raw-ID ranges after the global table's
         * same-pixel OR has already been applied. A present SDL table is
         * authoritative, including an empty property result. */
        for (i = 0; i < object_count && count < capacity; i++) {
            uint16_t raw = objects[i].raw;
            AiGlobalProperty item;
            if (raw > 0x07d0u && raw < 0x0fa0u) {
                item.ref = rich4_game_domain_ref(
                    GAME_DOMAIN_LAND, (int)raw - 0x07d1);
                if (item.ref.id >= s->land_count) continue;
            } else if (raw > 0x0fa0u && raw < 0x1770u) {
                item.ref = rich4_game_domain_ref(
                    GAME_DOMAIN_FACILITY, (int)raw - 0x0fa1);
                if (item.ref.id >= s->facility_count) continue;
            } else {
                continue;
            }
            item.order_x = objects[i].projected_x;
            item.order_y = objects[i].projected_y;
            out[count++] = item;
        }
        return count;
    }
    if (!ai_property_geometry_present(s))
        return -1;
    for (i = 0; i < s->land_count &&
                i < GAME_MAX_MAP_PROPERTIES; i++) {
        AiGlobalProperty item;
        int pos;
        if (count >= capacity) break;
        item.ref = rich4_game_domain_ref(GAME_DOMAIN_LAND, i);
        item.order_x = (int)s->tool_land_x[i] >> 5;
        item.order_y = (int)s->tool_land_y[i] >> 5;
        pos = count++;
        while (pos > 0 && ai_global_property_before(
                   &item, &out[pos - 1])) {
            out[pos] = out[pos - 1];
            pos--;
        }
        out[pos] = item;
    }
    for (i = 0; i < s->facility_count &&
                i < GAME_MAX_MAP_PROPERTIES; i++) {
        AiGlobalProperty item;
        int pos;
        if (count >= capacity) break;
        item.ref = rich4_game_domain_ref(GAME_DOMAIN_FACILITY, i);
        item.order_x = (int)s->tool_facility_x[i] >> 5;
        item.order_y = (int)s->tool_facility_y[i] >> 5;
        pos = count++;
        while (pos > 0 && ai_global_property_before(
                   &item, &out[pos - 1])) {
            out[pos] = out[pos - 1];
            pos--;
        }
        out[pos] = item;
    }
    return count;
}

/* 0x41E779 (original card ID 2) selects an actor that is both visible on a
 * route node and substantially richer than the current player. The preferred
 * relation target gets the stricter 30,000 * price_index ceiling; otherwise
 * the original ascending player-index scan overwrites the target for every
 * match, so the highest-index matching visible player wins. */
static int ai_average_poverty_command(const GameState *s,
                                      const uint8_t *player_on_map,
                                      int p, GameCommand *out)
{
    int preferred;
    int target = -1;
    int i;
    int64_t actor_cash = s->players[p].cash;

    if (player_on_map == NULL) return 0;
    preferred = relation_preferred_player(s, p);
    if (preferred >= 0 && preferred < s->player_count &&
        player_on_map[preferred] != 0 &&
        (int64_t)s->players[preferred].cash <
            (int64_t)s->price_index * 30000 &&
        (int64_t)s->players[preferred].cash > actor_cash * 2)
        target = preferred;

    if (target < 0) {
        for (i = 0; i < s->player_count; i++) {
            int64_t cash;
            if (i == p || player_on_map[i] == 0 ||
                (s->players[i].play_state & GAME_PLAYER_PLAY_STATE_MASK) == 0)
                continue;
            cash = s->players[i].cash;
            if (cash < (int64_t)s->price_index * 50000 &&
                cash > actor_cash * 3)
                target = i;
        }
    }
    if (target < 0) return 0;
    *out = rich4_game_domain_command(
        GAME_COMMAND_USE_CARD, GAME_COMMAND_SOURCE_AI, p,
        rich4_game_domain_ref(GAME_DOMAIN_CARD, 1),
        rich4_game_domain_ref(GAME_DOMAIN_PLAYER, target), 0);
    return 1;
}

/* 0x4202D2 (original card ID 26) chooses a visible, active player with
 * enough cash to tax. The relation-preferred player is accepted at the
 * 30,000 * price_index threshold; otherwise the original ascending scan
 * overwrites the target for every match at 50,000, leaving the highest-index
 * matching visible player. */
static int ai_tax_command(const GameState *s, const uint8_t *player_on_map,
                          int p, GameCommand *out)
{
    int preferred;
    int target = -1;
    int i;
    if (player_on_map == NULL) return 0;
    preferred = relation_preferred_player(s, p);
    if (preferred >= 0 && player_on_map[preferred] != 0 &&
        (int64_t)s->players[preferred].cash >
            (int64_t)s->price_index * 30000)
        target = preferred;
    if (target < 0) {
        for (i = 0; i < s->player_count; i++) {
            if (i == p || player_on_map[i] == 0 ||
                (s->players[i].play_state & GAME_PLAYER_PLAY_STATE_MASK) == 0)
                continue;
            if ((int64_t)s->players[i].cash >
                    (int64_t)s->price_index * 50000)
                target = i;
        }
    }
    if (target < 0) return 0;
    *out = rich4_game_domain_command(
        GAME_COMMAND_USE_CARD, GAME_COMMAND_SOURCE_AI, p,
        rich4_game_domain_ref(GAME_DOMAIN_CARD, 25),
        rich4_game_domain_ref(GAME_DOMAIN_PLAYER, target), 0);
    return 1;
}

/* 0x4207CC (original card ID 29) first chooses the active player with the
 * greatest number of owned land and facility records.  Only after that scan
 * does the original check the visible-player candidate set: the winner must
 * not be the relation-preferred player or already linked to the actor.
 * Equal counts keep the lower player index; an excluded winner does not fall
 * through to the next candidate. */
static int ai_alliance_command(const GameState *s,
                               const uint8_t *player_on_map,
                               int p, GameCommand *out)
{
    int preferred;
    int best = -1;
    int best_properties = 0;
    int i;
    if (player_on_map == NULL) return 0;
    preferred = relation_preferred_player(s, p);
    for (i = 0; i < s->player_count; i++) {
        int properties = 0;
        int j;
        if (i == p || (s->players[i].play_state &
                       GAME_PLAYER_PLAY_STATE_MASK) == 0)
            continue;
        for (j = 0; j < s->land_count; j++)
            if (s->land_owner[j] == i) properties++;
        for (j = 0; j < s->facility_count; j++)
            if (s->facility_owner[j] == i) properties++;
        if (properties <= best_properties) continue;
        best_properties = properties;
        best = i;
    }
    if (best < 0 || best == preferred || player_on_map[best] == 0 ||
        s->players[best].linked_player == p)
        return 0;
    *out = rich4_game_domain_command(
        GAME_COMMAND_USE_CARD, GAME_COMMAND_SOURCE_AI, p,
        rich4_game_domain_ref(GAME_DOMAIN_CARD, 28),
        rich4_game_domain_ref(GAME_DOMAIN_PLAYER, best), 0);
    return 1;
}

/* 0x41E9E2 (original card ID 3) only considers the property under the
 * actor's feet.  A relation target must exist before the original helper is
 * allowed to accept the property.  For ordinary land, either the preferred
 * player owns a built target of level 2 or higher, or the actor already owns
 * another member of the same group.  A built facility owned by another
 * player is sufficient.  The AI uses a strict affordability check: purchase
 * cost must be less than, not equal to, current cash. */
static int ai_purchase_command(const GameState *s,
                               GameDomainRef current_property,
                               int p, GameCommand *out)
{
    int preferred = relation_preferred_player(s, p);
    int price;
    int owner;
    int level;
    int i;
    int eligible = 0;

    if (preferred < 0) return 0;
    if (current_property.kind == GAME_DOMAIN_LAND &&
        current_property.id >= 0 && current_property.id < s->land_count) {
        owner = s->land_owner[current_property.id];
        level = s->land_level[current_property.id];
        if (owner < 0 || owner >= s->player_count || owner == p || level == 0)
            return 0;
        if (owner == preferred && level >= 2) {
            eligible = 1;
        } else if (s->land_group[current_property.id] >= 0) {
            for (i = 0; i < s->land_count; i++) {
                if (i == current_property.id ||
                    s->land_group[i] != s->land_group[current_property.id] ||
                    s->land_owner[i] != p) continue;
                eligible = 1;
                break;
            }
        }
        if (!eligible || !rich4_land_purchase_price(
                s, current_property.id, &price) ||
            (int64_t)price >= s->players[p].cash)
            return 0;
    } else if (current_property.kind == GAME_DOMAIN_FACILITY &&
               current_property.id >= 0 &&
               current_property.id < s->facility_count) {
        owner = s->facility_owner[current_property.id];
        level = s->facility_level[current_property.id];
        if (owner < 0 || owner >= s->player_count || owner == p || level == 0)
            return 0;
        if (!rich4_facility_purchase_price(s, current_property.id, &price) ||
            (int64_t)price >= s->players[p].cash)
            return 0;
    } else {
        return 0;
    }
    *out = rich4_game_domain_command(
        GAME_COMMAND_USE_CARD, GAME_COMMAND_SOURCE_AI, p,
        rich4_game_domain_ref(GAME_DOMAIN_CARD, 2), current_property, 0);
    return 1;
}

/* 0x41EF26: any opponent's level >=3 asset, or the relation target's
 * level >=2 asset, under the actor's feet. */
static int ai_auction_command(const GameState *s,
                              GameDomainRef current_property,
                              int p, GameCommand *out)
{
    int preferred = relation_preferred_player(s, p);
    int owner;
    int level;
    if (current_property.kind == GAME_DOMAIN_LAND &&
        current_property.id >= 0 && current_property.id < s->land_count) {
        owner = s->land_owner[current_property.id];
        level = s->land_level[current_property.id];
    } else if (current_property.kind == GAME_DOMAIN_FACILITY &&
               current_property.id >= 0 &&
               current_property.id < s->facility_count) {
        owner = s->facility_owner[current_property.id];
        level = s->facility_level[current_property.id];
    } else {
        return 0;
    }
    if (owner < 0 || owner >= s->player_count || owner == p || level < 0)
        return 0;
    if (level < 3 && (owner != preferred || level < 2)) return 0;
    *out = rich4_game_domain_command(
        GAME_COMMAND_USE_CARD, GAME_COMMAND_SOURCE_AI, p,
        rich4_game_domain_ref(GAME_DOMAIN_CARD, 7), current_property, 0);
    return 1;
}

/* 0x41EAE2 (original card ID 4) compares the current owned low-level
 * property with a more valuable, more developed property from the visible
 * object table.  GameState already has the typed land/facility records, so
 * keep only the direct property branch: no dynamic/special object-table
 * fallback is inferred.  The shared land helper's relation/group checks are
 * represented explicitly and the command carries the current property in
 * value, as required by rich4_card_apply_property_card(). */
static int ai_swap_land_command(const GameState *s,
                                const GameAiGlobalProjection *projection,
                                GameDomainRef current_property,
                                int p, GameCommand *out)
{
    GameDomainRef visible[GAME_MAX_MAP_PROPERTIES * 2];
    int land_candidates[GAME_MAX_MAP_PROPERTIES];
    int facility_candidates[GAME_MAX_MAP_PROPERTIES];
    int visible_count;
    int land_candidate_count = 0;
    int facility_candidate_count = 0;
    int preferred;
    int source_owner;
    int source_level;
    int i;
    int r;
    int target = -1;
    int source_group = -1;

    if (s == NULL || out == NULL || p < 0 || p >= s->player_count)
        return 0;
    preferred = relation_preferred_player(s, p);
    /* 0x41E8E6 rejects before inspecting either asset class without R. */
    if (preferred < 0) return 0;
    visible_count = ai_collect_card_global_property_refs(
        s, p, projection, visible,
        (int)(sizeof(visible) / sizeof(visible[0])));
    if (visible_count >= 0) {
        for (r = 0; r < visible_count; r++) {
            if (visible[r].kind == GAME_DOMAIN_LAND &&
                visible[r].id >= 0 && visible[r].id < s->land_count &&
                land_candidate_count < GAME_MAX_MAP_PROPERTIES)
                land_candidates[land_candidate_count++] = visible[r].id;
            else if (visible[r].kind == GAME_DOMAIN_FACILITY &&
                     visible[r].id >= 0 &&
                     visible[r].id < s->facility_count &&
                     facility_candidate_count < GAME_MAX_MAP_PROPERTIES)
                facility_candidates[facility_candidate_count++] =
                    visible[r].id;
        }
    } else {
        for (i = 0; i < s->land_count &&
                    land_candidate_count < GAME_MAX_MAP_PROPERTIES; i++)
            land_candidates[land_candidate_count++] = i;
        for (i = 0; i < s->facility_count &&
                    facility_candidate_count < GAME_MAX_MAP_PROPERTIES; i++)
            facility_candidates[facility_candidate_count++] = i;
    }
    if (current_property.kind == GAME_DOMAIN_LAND &&
        current_property.id >= 0 && current_property.id < s->land_count) {
        source_owner = s->land_owner[current_property.id];
        source_level = s->land_level[current_property.id];
        source_group = s->land_group[current_property.id];
        if (source_owner != p || source_level > 1) return 0;
        /* The original refuses this branch when the actor already owns
         * another member of the source name group. */
        for (i = 0; i < s->land_count; i++)
            if (i != current_property.id && source_group >= 0 &&
                s->land_group[i] == source_group && s->land_owner[i] == p)
                return 0;
        for (r = 0; r < land_candidate_count; r++) {
            i = land_candidates[r];
            int owner = s->land_owner[i];
            int actor_group_member = 0;
            int j;
            if (i == current_property.id ||
                s->land_group[i] == source_group ||
                owner < 0 || owner >= s->player_count || owner == p ||
                (s->players[owner].play_state &
                 GAME_PLAYER_PLAY_STATE_MASK) == 0 ||
                s->land_level[i] <= source_level ||
                s->land_price[i] <= s->land_price[current_property.id])
                continue;
            for (j = 0; j < s->land_count; j++)
                if (s->land_group[j] == s->land_group[i] &&
                    s->land_owner[j] == p) {
                    actor_group_member = 1;
                    break;
                }
            if (actor_group_member ||
                (owner == preferred && s->land_level[i] >= 2)) {
                target = i;
                break;
            }
        }
    } else if (current_property.kind == GAME_DOMAIN_FACILITY &&
               current_property.id >= 0 &&
               current_property.id < s->facility_count) {
        source_owner = s->facility_owner[current_property.id];
        source_level = s->facility_level[current_property.id];
        if (source_owner != p || source_level > 1) return 0;
        for (r = 0; r < facility_candidate_count; r++) {
            i = facility_candidates[r];
            int owner = s->facility_owner[i];
            if (i == current_property.id || owner < 0 ||
                owner >= s->player_count || owner == p ||
                (s->players[owner].play_state &
                 GAME_PLAYER_PLAY_STATE_MASK) == 0 ||
                s->facility_level[i] <= source_level ||
                s->facility_price[i] <=
                    s->facility_price[current_property.id])
                continue;
            target = i;
            break;
        }
    } else {
        return 0;
    }
    if (target < 0) return 0;
    *out = rich4_game_domain_command(
        GAME_COMMAND_USE_CARD, GAME_COMMAND_SOURCE_AI, p,
        rich4_game_domain_ref(GAME_DOMAIN_CARD, 3),
        current_property.kind == GAME_DOMAIN_LAND
            ? rich4_game_domain_ref(GAME_DOMAIN_LAND, target)
            : rich4_game_domain_ref(GAME_DOMAIN_FACILITY, target),
        current_property.id);
    if (current_property.kind == GAME_DOMAIN_FACILITY)
        out->flags |= GAME_CARD_FLAG_SECONDARY_FACILITY;
    return rich4_card_inspect(s, out) == GAME_CARD_USE_APPLIED;
}

static int ai_rebuild_command(const GameState *s,
                              GameDomainRef current_property,
                              int p, Rich4Rng *rng, GameCommand *out)
{
    GameDomainRef target;
    int owner, level, type, i;
    target = current_property;
    if (target.kind == GAME_DOMAIN_LAND &&
        target.id >= 0 && target.id < s->land_count) {
        owner = s->land_owner[target.id];
        level = s->land_level[target.id];
        type = s->land_type[target.id];
        if (owner != p) return 0;
        if (type != 0) {
            for (i = 0; i < s->land_count; i++)
                if (i != target.id && s->land_group[i] ==
                        s->land_group[target.id] && s->land_owner[i] == p)
                    break;
            if (i == s->land_count) return 0;
        } else {
            if (level != 1) return 0;
            if (s->players[p].ai_personality != GAME_AI_GOOD_CHILD) {
                for (i = 0; i < s->land_count; i++)
                    if (i != target.id && s->land_group[i] ==
                            s->land_group[target.id] &&
                        (s->land_owner[i] == p || s->land_owner[i] < 0))
                        return 0;
            }
        }
        *out = rich4_game_domain_command(
            GAME_COMMAND_USE_CARD, GAME_COMMAND_SOURCE_AI, p,
            rich4_game_domain_ref(GAME_DOMAIN_CARD, 6), target, 0);
        return 1;
    }
    if (target.kind == GAME_DOMAIN_FACILITY &&
        target.id >= 0 && target.id < s->facility_count) {
        int preferred = relation_preferred_player(s, p);
        owner = s->facility_owner[target.id];
        level = s->facility_level[target.id];
        type = s->facility_type[target.id];
        if (owner == p && type == 0 && level == 1) {
            *out = rich4_game_domain_command(
                GAME_COMMAND_USE_CARD, GAME_COMMAND_SOURCE_AI, p,
                rich4_game_domain_ref(GAME_DOMAIN_CARD, 6), target, 0);
            out->value2 = rich4_rng_original_state_range(rng, 4) + 1;
            return 1;
        }
        if (owner < 0 || owner == p || type == 0 ||
            (level < 3 && (owner != preferred || level < 2))) return 0;
        *out = rich4_game_domain_command(
            GAME_COMMAND_USE_CARD, GAME_COMMAND_SOURCE_AI, p,
            rich4_game_domain_ref(GAME_DOMAIN_CARD, 6), target, 0);
        out->value2 = 0;
        return 1;
    }
    return 0;
}

typedef struct AiPropertyCandidate {
    int id;
    int level;
    int price;
} AiPropertyCandidate;

static void keep_property_candidate(AiPropertyCandidate *candidate,
                                    int id, int level, int price)
{
    if (candidate == NULL || level < candidate->level ||
        (level == candidate->level && price <= candidate->price)) return;
    candidate->id = id;
    candidate->level = level;
    candidate->price = price;
}

static int ai_monster_command(const GameState *s,
                              const GameAiGlobalProjection *projection,
                              int p, GameCommand *out)
{
    GameDomainRef visible[GAME_MAX_MAP_PROPERTIES * 2];
    unsigned char visible_land[GAME_MAX_MAP_PROPERTIES];
    unsigned char visible_facility[GAME_MAX_MAP_PROPERTIES];
    AiPropertyCandidate land[GAME_MAX_PLAYERS];
    AiPropertyCandidate facility[GAME_MAX_PLAYERS];
    AiPropertyCandidate best_land = {-1, 0, 0};
    AiPropertyCandidate best_facility = {-1, 0, 0};
    int visible_count;
    int preferred = relation_preferred_player(s, p);
    int i;
    int r;
    memset(visible_land, 0, sizeof(visible_land));
    memset(visible_facility, 0, sizeof(visible_facility));
    visible_count = ai_collect_card_global_property_refs(
        s, p, projection, visible,
        (int)(sizeof(visible) / sizeof(visible[0])));
    if (visible_count >= 0) {
        for (r = 0; r < visible_count; r++) {
            if (visible[r].kind == GAME_DOMAIN_LAND &&
                visible[r].id >= 0 && visible[r].id < GAME_MAX_MAP_PROPERTIES)
                visible_land[visible[r].id] = 1;
            else if (visible[r].kind == GAME_DOMAIN_FACILITY &&
                     visible[r].id >= 0 &&
                     visible[r].id < GAME_MAX_MAP_PROPERTIES)
                visible_facility[visible[r].id] = 1;
        }
    }
    memset(land, 0, sizeof(land));
    memset(facility, 0, sizeof(facility));
    for (i = 0; i < GAME_MAX_PLAYERS; i++) {
        land[i].id = -1;
        facility[i].id = -1;
    }
    for (r = 0; r < (visible_count >= 0 ? visible_count : s->land_count); r++) {
        if (visible_count >= 0 && visible[r].kind != GAME_DOMAIN_LAND) continue;
        i = visible_count >= 0 ? visible[r].id : r;
        int owner = s->land_owner[i];
        if (visible_count >= 0 && !visible_land[i]) continue;
        if (owner < 0 || owner >= s->player_count || owner == p ||
            s->land_level[i] < 3) continue;
        keep_property_candidate(&land[owner], i, s->land_level[i],
                                s->land_price[i]);
    }
    for (r = 0; r < (visible_count >= 0 ? visible_count : s->facility_count); r++) {
        if (visible_count >= 0 && visible[r].kind != GAME_DOMAIN_FACILITY) continue;
        i = visible_count >= 0 ? visible[r].id : r;
        int owner = s->facility_owner[i];
        if (visible_count >= 0 && !visible_facility[i]) continue;
        if (owner < 0 || owner >= s->player_count || owner == p ||
            s->facility_level[i] < 3) continue;
        keep_property_candidate(&facility[owner], i, s->facility_level[i],
                                s->facility_price[i]);
    }
    if (preferred >= 0) {
        GameDomainRef target;
        if (facility[preferred].id >= 0)
            target = rich4_game_domain_ref(GAME_DOMAIN_FACILITY,
                                           facility[preferred].id);
        else if (land[preferred].id >= 0)
            target = rich4_game_domain_ref(GAME_DOMAIN_LAND,
                                           land[preferred].id);
        else
            target = none_ref();
        if (!rich4_game_domain_ref_is_none(target)) {
            *out = rich4_game_domain_command(
                GAME_COMMAND_USE_CARD, GAME_COMMAND_SOURCE_AI, p,
                rich4_game_domain_ref(GAME_DOMAIN_CARD, 10), target, 0);
            return 1;
        }
    }
    for (i = 0; i < s->player_count; i++) {
        if (i == p || (s->players[i].play_state &
                       GAME_PLAYER_PLAY_STATE_MASK) == 0) continue;
        if (land[i].level >= 4)
            keep_property_candidate(&best_land, land[i].id,
                                    land[i].level, land[i].price);
        if (facility[i].level >= 3)
            keep_property_candidate(&best_facility, facility[i].id,
                                    facility[i].level, facility[i].price);
    }
    if (best_facility.id >= 0 || best_land.id >= 0) {
        GameDomainRef target = best_facility.id >= 0
            ? rich4_game_domain_ref(GAME_DOMAIN_FACILITY, best_facility.id)
            : rich4_game_domain_ref(GAME_DOMAIN_LAND, best_land.id);
        *out = rich4_game_domain_command(
            GAME_COMMAND_USE_CARD, GAME_COMMAND_SOURCE_AI, p,
            rich4_game_domain_ref(GAME_DOMAIN_CARD, 10), target, 0);
        return 1;
    }
    return 0;
}

/* 0x41F6A9 (original card ID 12) first calls the 0x41F400 monster selector,
 * then scans the shared 0x40A45C(-1) table in row-major order. Preserve the
 * selector's land/facility gates before its dynamic slot checks. */
static int ai_demolish_object_fallback_command(
    const GameState *s, const GameAiGlobalProjection *projection, int p,
    GameCommand *out)
{
    AiProjectedObject visible[AI_PROJECTED_OBJECT_CAPACITY];
    int visible_count = -1;
    int i;

    if (s == NULL || out == NULL || p < 0 || p >= s->player_count)
        return 0;

    if (projection != NULL && projection->object_table_available) {
        visible_count = ai_prepare_global_player_masks(
            s, visible, AI_PROJECTED_OBJECT_CAPACITY, projection);
        if (visible_count < 0) return 0;
    }

    for (i = 0; i < (visible_count >= 0
                         ? visible_count : GAME_TOOL_PLACED_OBJECT_COUNT); i++) {
        int placement_slot;
        const struct GameToolPlacedObjectState *object;
        int owner = -1;
        int property_kind = 0;
        int property_id;
        uint16_t raw;

        if (visible_count >= 0) {
            int dynamic_slot;
            raw = visible[i].raw;
            if (raw > 0x07d0u && raw < 0x0fa0u) {
                int land_id = (int)raw - 0x07d1;
                int owned_built_land_count = 0;
                int land;

                if (s->players[p].ai_personality == 0 ||
                    land_id < 0 || land_id >= s->land_count)
                    continue;
                owner = s->land_owner[land_id];
                if (owner < 0 || owner >= s->player_count || owner == p ||
                    s->land_type[land_id] == 0)
                    continue;
                for (land = 0; land < s->land_count; land++)
                    if (s->land_type[land] != 0 &&
                        s->land_owner[land] == owner)
                        owned_built_land_count++;
                if (owned_built_land_count < 4) continue;

                *out = rich4_game_domain_command(
                    GAME_COMMAND_USE_CARD, GAME_COMMAND_SOURCE_AI, p,
                    rich4_game_domain_ref(GAME_DOMAIN_CARD, 11),
                    rich4_game_domain_ref(GAME_DOMAIN_LAND, land_id), 0);
                return rich4_card_inspect(s, out) ==
                       GAME_CARD_USE_APPLIED;
            }
            if (raw > 0x0fa0u && raw < 0x1770u) {
                int facility_id = (int)raw - 0x0fa1;

                if ((s->players[p].traffic_method & 3) == 0 || facility_id < 0 ||
                    facility_id >= s->facility_count ||
                    s->facility_type[facility_id] != 3 ||
                    s->facility_level[facility_id] != 1)
                    continue;
                owner = s->facility_owner[facility_id];
                if (owner < 0 || owner >= s->player_count || owner == p)
                    continue;

                *out = rich4_game_domain_command(
                    GAME_COMMAND_USE_CARD, GAME_COMMAND_SOURCE_AI, p,
                    rich4_game_domain_ref(GAME_DOMAIN_CARD, 11),
                    rich4_game_domain_ref(GAME_DOMAIN_FACILITY,
                                          facility_id), 0);
                return rich4_card_inspect(s, out) ==
                       GAME_CARD_USE_APPLIED;
            }
            if ((raw & 0x8000u) == 0 || ((raw >> 8) & 0x3fu) == 0)
                continue;
            dynamic_slot = (int)((raw >> 8) & 0x3fu) - 1;
            if (dynamic_slot < 16 ||
                dynamic_slot >= GAME_MAP_DYNAMIC_OBJECT_COUNT)
                continue;
            placement_slot = dynamic_slot - 16;
            if (placement_slot < 0 ||
                placement_slot >= GAME_TOOL_PLACED_OBJECT_COUNT)
                continue;
        } else {
            placement_slot = i;
        }
        object = &s->tool_placed_objects[placement_slot];

        if ((object->kind != 0x10 && object->kind != 0x11) ||
            !rich4_tool_valid_node(s, object->node))
            continue;
        raw = s->route_object_raw[object->node];
        if (raw > 0x7d0 && raw < 0xfa0) {
            property_kind = GAME_DOMAIN_LAND;
            property_id = (int)raw - 0x7d1;
            if (property_id >= 0 && property_id < s->land_count)
                owner = s->land_owner[property_id];
        } else if (raw > 0xfa0 && raw < 0x1770) {
            property_kind = GAME_DOMAIN_FACILITY;
            property_id = (int)raw - 0xfa1;
            if (property_id >= 0 && property_id < s->facility_count)
                owner = s->facility_owner[property_id];
        }
        if (property_kind == 0 || owner < 0) continue;
        /* 0x41F6A9: type 16 rejects the current player's own property;
         * type 17 accepts only the current player's property. Type 18 has no
         * candidate branch in the original handler. */
        if ((object->kind == 0x10 && owner == p) ||
            (object->kind == 0x11 && owner != p))
            continue;
        *out = rich4_game_domain_command(
            GAME_COMMAND_USE_CARD, GAME_COMMAND_SOURCE_AI, p,
            rich4_game_domain_ref(GAME_DOMAIN_CARD, 11),
            rich4_game_domain_ref(GAME_DOMAIN_MAP_OBJECT, placement_slot), 0);
        if (rich4_card_inspect(s, out) == GAME_CARD_USE_APPLIED)
            return 1;
    }
    return 0;
}

static int ai_demolish_command(const GameState *s,
                               const GameAiGlobalProjection *projection,
                               int p, GameCommand *out)
{
    if (s == NULL || out == NULL) return 0;
    if (ai_monster_command(s, projection, p, out)) {
        out->subject = rich4_game_domain_ref(GAME_DOMAIN_CARD, 11);
        if (rich4_card_inspect(s, out) == GAME_CARD_USE_APPLIED)
            return 1;
    }
    return ai_demolish_object_fallback_command(s, projection, p, out);
}

static int ai_collect_linear_route_window(const GameState *s, int p,
                                          int limit, Rich4Rng *rng,
                                          int out_nodes[8]);
static int ai_collect_original_route_window(
    const GameState *s, int start_node, int previous_node, int limit,
    Rich4Rng *rng, int out_nodes[8], int *out_had_branch);
static int ai_collect_route_window(const GameState *s, int p, int limit,
                                   int reverse, Rich4Rng *rng,
                                   int out_nodes[8]);
static int ai_card30_direct_gate(const GameState *s, int p, Rich4Rng *rng,
                                 const GameAiGlobalProjection *projection,
                                 int *out_target);

/* fcn_00420efa uses the four records selected by fcn_0040b221. */
static int ai_collect_machine_doll_window(const GameState *s, int p,
                                          Rich4Rng *rng, int out_nodes[4],
                                          int *out_branched)
{
    int route_nodes[8];
    int branched = 0;
    int count;
    int i;

    if (out_branched != NULL) *out_branched = 0;
    if (s == NULL || out_nodes == NULL || p < 0 || p >= s->player_count)
        return 0;
    count = ai_collect_original_route_window(
        s, s->players[p].node, s->players[p].previous_node, 4,
        rng, route_nodes, &branched);
    if (count < 0 || count > 4) return 0;
    for (i = 0; i < count; i++) out_nodes[i] = route_nodes[i];
    if (out_branched != NULL) *out_branched = branched;
    return count;
}

/* ref_00496d08 is initialized from ref_0047ed3c.  The route record stores a
 * one-based selector in bits 16..21; the table values are the original map
 * object categories used by 0x420efa. */
static int ai_original_route_object_type(const GameState *s, int node)
{
    static const unsigned char type_by_selector[46] = {
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 15,
        16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
        17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
        18, 18, 18, 18, 18, 18, 18, 18, 18, 18
    };
    unsigned int selector;

    if (s == NULL || !rich4_tool_valid_node(s, node)) return -1;
    selector = (s->tool_route_flags[node] >> 16) & 0x3fu;
    if (selector == 0) return 0;
    if (selector > sizeof(type_by_selector)) return -1;
    return type_by_selector[selector - 1];
}

static int64_t ai_machine_doll_land_group_value(const GameState *s, int id)
{
    int owner;
    int group;
    int64_t value = 0;
    int i;

    if (s == NULL || id < 0 || id >= s->land_count) return 0;
    owner = s->land_owner[id];
    group = s->land_group[id];
    /* fcn_00419744 receives the candidate record's name pointer, but its
     * internal scan sums only ordinary type-0 records with that name.  The
     * candidate record itself is not required to be type 0. */
    if (owner < 0 || group < 0) return 0;
    for (i = 0; i < s->land_count; i++) {
        int level;
        if (s->land_group[i] != group || s->land_owner[i] != owner)
            continue;
        if (s->land_type[i] != 0) continue;
        level = s->land_level[i];
        if (level < 0 || level >= 6) continue;
        value += s->land_toll_by_level[i][level];
    }
    return value * (s->price_index > 0 ? s->price_index : 1);
}

static int ai_roadblock_land_name_group(
    const GameState *s, const GameAiGlobalProjection *projection, int id);
static int64_t ai_roadblock_land_group_value(
    const GameState *s, const GameAiGlobalProjection *projection,
    int player, int group);

/* 0x420EFA accepts the first relevant obstacle on an unbranched four-step
 * path: categories 5/6/7/8/11, category 17 on self property, or category 16
 * on valuable external property. Facility value is fixed at 10000000. */
static int ai_machine_doll_route_gate(
    const GameState *s, int p, Rich4Rng *rng,
    const GameAiGlobalProjection *projection)
{
    int nodes[4];
    int count;
    int branched = 0;
    int i;

    count = ai_collect_machine_doll_window(s, p, rng, nodes, &branched);
    if (count <= 0) return -1;
    if (branched) return 0;
    for (i = 0; i < count; i++) {
        int object_type = ai_original_route_object_type(s, nodes[i]);
        uint16_t raw = s->route_object_raw[nodes[i]];
        int owner = -1;
        int64_t value = 0;

        if (object_type < 0) return -1;
        if (object_type == 5 || object_type == 6 || object_type == 7 ||
            object_type == 8 || object_type == 11) return 1;
        if (object_type != 16 && object_type != 17) continue;
        if (raw > 0x7d0 && raw < 0xfa0) {
            int id = (int)raw - 0x7d1;
            int group;
            if (id >= s->land_count) return -1;
            owner = s->land_owner[id];
            if (object_type == 16 && owner >= 0 && owner != p) {
                group = ai_roadblock_land_name_group(s, projection, id);
                if (group < 0) return -1;
                value = ai_roadblock_land_group_value(s, projection, owner, group);
            }
        } else if (raw > 0xfa0 && raw < 0x1770) {
            int id = (int)raw - 0xfa1;
            if (id >= s->facility_count) return -1;
            owner = s->facility_owner[id];
            value = 10000000;
        }
        if ((object_type == 17 && owner == p) ||
            (object_type == 16 && owner >= 0 && owner != p &&
             value > 3000LL * s->price_index)) return 1;
    }
    return 0;
}

/* 0x40A0B1/0x40A45C rebuilds a row-major 29x29 tile table around the active
 * player's map position.  This value helper intentionally keeps only owned
 * map records; a record later in the land/facility scan replaces an earlier
 * record in the same tile.  Callers that need foreign or unowned candidates
 * must use the full raw-object projection.  Return -1 when the app has not
 * supplied property coordinates so callers can retain their typed fallback. */
static int ai_collect_visible_property_refs(const GameState *s, int p,
                                            GameDomainRef *out, int capacity)
{
    int center_node;
    int center_x;
    int center_y;
    int tile_x;
    int tile_y;
    int i;
    int count = 0;
    int have_property_geometry = 0;

    if (s == NULL || out == NULL || capacity <= 0 || p < 0 ||
        p >= s->player_count || s->tool_route_node_count <= 1 ||
        !rich4_tool_valid_node(s, s->players[p].node))
        return -1;
    for (i = 0; i < s->land_count && i < GAME_MAX_MAP_PROPERTIES; i++) {
        if (s->tool_land_x[i] != 0 || s->tool_land_y[i] != 0) {
            have_property_geometry = 1;
            break;
        }
    }
    if (!have_property_geometry) {
        for (i = 0;
             i < s->facility_count && i < GAME_MAX_MAP_PROPERTIES; i++) {
            if (s->tool_facility_x[i] != 0 ||
                s->tool_facility_y[i] != 0) {
                have_property_geometry = 1;
                break;
            }
        }
    }
    if (!have_property_geometry) return -1;

    center_node = s->players[p].node;
    center_x = s->tool_route_x[center_node];
    center_y = s->tool_route_y[center_node];
    for (tile_y = 0; tile_y < 29; tile_y++) {
        for (tile_x = 0; tile_x < 29; tile_x++) {
            GameDomainRef last = none_ref();
            for (i = 0; i < s->land_count && i < GAME_MAX_MAP_PROPERTIES;
                 i++) {
                int projected_x;
                int projected_y;
                if (s->land_owner[i] < 0) continue;
                projected_x = ((int)s->tool_land_x[i] >> 5) -
                              ((int)center_x >> 5) + 14;
                projected_y = ((int)s->tool_land_y[i] >> 5) -
                              ((int)center_y >> 5) + 14;
                if (projected_x == tile_x && projected_y == tile_y)
                    last = rich4_game_domain_ref(GAME_DOMAIN_LAND, i);
            }
            for (i = 0;
                 i < s->facility_count && i < GAME_MAX_MAP_PROPERTIES; i++) {
                int projected_x;
                int projected_y;
                if (s->facility_owner[i] < 0) continue;
                projected_x = ((int)s->tool_facility_x[i] >> 5) -
                              ((int)center_x >> 5) + 14;
                projected_y = ((int)s->tool_facility_y[i] >> 5) -
                              ((int)center_y >> 5) + 14;
                if (projected_x == tile_x && projected_y == tile_y)
                    last = rich4_game_domain_ref(GAME_DOMAIN_FACILITY, i);
            }
            if (rich4_game_domain_ref_is_none(last)) continue;
            if (count < capacity) out[count++] = last;
        }
    }
    return count;
}

/* fcn_00421CB6 calls fcn_00409EF9 rather than the short forward-route
 * helper. It compacts the visible route-node table into a 440x440 window and
 * transfer handler then keeps the highest qualifying unowned property from
 * that table. The original keeps the first candidate at the highest level.
 * A route node is already the typed PLAYER -> ROUTE_NODE destination. Without
 * an SDL viewport, core-only states use the typed-coordinate reconstruction.
 */
static int ai_transfer_visible_property_gate(
    const GameState *s, int p,
    const GameAiGlobalProjection *projection, int *out_node)
{
    int center_node;
    int center_x;
    int center_y;
    int have_geometry = 0;
    int best_level = -1;
    int best_node = -1;
    int level;
    int64_t price;
    int i;

    if (out_node != NULL) *out_node = -1;
    if (s == NULL || p < 0 || p >= s->player_count ||
        s->tool_route_node_count <= 1 ||
        !rich4_tool_valid_node(s, s->players[p].node))
        return -2;

    /* 0x421CB6 calls 0x409EF9, which projects every qualifying route record
     * into a 440x440 table, lets later route records replace earlier records
     * at the same pixel, and scans the compacted table in row-major order.
     * Reuse the same route-table projection as the route-target handlers when
     * an SDL viewport is available. The rendered-object table is a different
     * original data path and is deliberately not used here. */
    if (projection != NULL) {
        int nodes[GAME_MAX_MAP_ROUTE_NODES];
        int count = ai_collect_global_route_nodes(
            s, projection, nodes, GAME_MAX_MAP_ROUTE_NODES);
        if (count < 0) return -2;
        for (i = 0; i < count; i++) {
            uint16_t raw = s->route_object_raw[nodes[i]];
            int id;

            if (raw > 0x7d0u && raw < 0xfa0u) {
                id = (int)raw - 0x7d1;
                if (id < 0 || id >= s->land_count) return -2;
                level = s->land_level[id];
                price = (int64_t)s->land_price_per_level[id] *
                    s->price_index;
                if (s->land_owner[id] != -1 || s->land_type[id] != 0 ||
                    level < 3 || price >= s->players[p].cash)
                    continue;
            } else if (raw > 0xfa0u && raw < 0x1770u) {
                id = (int)raw - 0xfa1;
                if (id < 0 || id >= s->facility_count) return -2;
                level = s->facility_level[id];
                price = (int64_t)s->facility_charge_by_level[id][0] *
                    s->price_index;
                if (s->facility_owner[id] != -1 ||
                    s->facility_type[id] == 0 || level < 3 ||
                    price >= s->players[p].cash)
                    continue;
            } else {
                continue;
            }

            /* The original only replaces the current result for a strictly
             * higher level, so equal-level ties retain the first row-major
             * candidate. The table item itself is the destination node. */
            if (level > best_level) {
                best_level = level;
                best_node = nodes[i];
            }
        }
        if (best_node >= 0) {
            if (out_node != NULL) *out_node = best_node;
            return 1;
        }
        return 0;
    }

    for (i = 0; i < s->land_count && i < GAME_MAX_MAP_PROPERTIES; i++) {
        if (s->tool_land_x[i] != 0 || s->tool_land_y[i] != 0) {
            have_geometry = 1;
            break;
        }
    }
    if (!have_geometry) {
        for (i = 0;
             i < s->facility_count && i < GAME_MAX_MAP_PROPERTIES; i++) {
            if (s->tool_facility_x[i] != 0 ||
                s->tool_facility_y[i] != 0) {
                have_geometry = 1;
                break;
            }
        }
    }
    if (!have_geometry) {
        GameAiGlobalProjection centered;
        memset(&centered, 0, sizeof(centered));
        centered.center_world_x = s->tool_route_x[s->players[p].node];
        centered.center_world_y = s->tool_route_y[s->players[p].node];
        return ai_transfer_visible_property_gate(s, p, &centered, out_node);
    }

    center_node = s->players[p].node;
    center_x = s->tool_route_x[center_node];
    center_y = s->tool_route_y[center_node];
    /* 0x409EF9 writes the last land/facility record at each tile and then
     * emits the resulting table row-major.  Rebuild that typed projection
     * directly instead of requiring the map route record to repeat the raw
     * property ID. */
    {
        int tile_y;
        int tile_x;
        for (tile_y = 0; tile_y < 29; tile_y++) {
            for (tile_x = 0; tile_x < 29; tile_x++) {
                GameDomainRef last = none_ref();
                int property_x;
                int property_y;
                int node;

                for (i = 0; i < s->land_count &&
                            i < GAME_MAX_MAP_PROPERTIES; i++) {
                    if (((s->tool_land_x[i] >> 5) - (center_x >> 5)) ==
                            tile_x - 14 &&
                        ((s->tool_land_y[i] >> 5) - (center_y >> 5)) ==
                            tile_y - 14)
                        last = rich4_game_domain_ref(GAME_DOMAIN_LAND, i);
                }
                for (i = 0; i < s->facility_count &&
                            i < GAME_MAX_MAP_PROPERTIES; i++) {
                    if (((s->tool_facility_x[i] >> 5) - (center_x >> 5)) ==
                            tile_x - 14 &&
                        ((s->tool_facility_y[i] >> 5) - (center_y >> 5)) ==
                            tile_y - 14)
                        last = rich4_game_domain_ref(GAME_DOMAIN_FACILITY, i);
                }
                if (rich4_game_domain_ref_is_none(last)) continue;
                if (last.kind == GAME_DOMAIN_LAND) {
                    property_x = s->tool_land_x[last.id];
                    property_y = s->tool_land_y[last.id];
                    level = s->land_level[last.id];
                    price = (int64_t)s->land_price_per_level[last.id] *
                        s->price_index;
                    if (s->land_owner[last.id] != -1 ||
                        s->land_type[last.id] != 0 || level < 3 ||
                        price >= s->players[p].cash)
                        continue;
                } else {
                    property_x = s->tool_facility_x[last.id];
                    property_y = s->tool_facility_y[last.id];
                    level = s->facility_level[last.id];
                    /* 0x421CB6 reads facility +0x24, the level-0 charge
                     * entry, for an unowned level-3+ facility. */
                    price = (int64_t)s->facility_charge_by_level[last.id][0] *
                        s->price_index;
                    if (s->facility_owner[last.id] != -1 ||
                        s->facility_type[last.id] == 0 || level < 3 ||
                        price >= s->players[p].cash)
                        continue;
                }

                /* The command model still needs a route node. Keep the
                 * visible-table semantics even when a static property is
                 * not represented by a route node, but emit a target only
                 * when its coordinates can be mapped back safely. */
                node = -1;
                for (i = 1; i < s->tool_route_node_count; i++) {
                    if (s->tool_route_x[i] == property_x &&
                        s->tool_route_y[i] == property_y) {
                        node = i;
                        break;
                    }
                }
                if (level > best_level) {
                    best_level = level;
                    best_node = node;
                }
            }
        }
    }
    if (best_level >= 0 && best_node >= 0) {
        if (out_node != NULL) *out_node = best_node;
        return 1;
    }
    if (best_level >= 0) return -2;
    return 0;
}

static int ai_price_increase_scan_refs(const GameState *s, int p,
                                       const GameDomainRef *refs, int count,
                                       int scan_register,
                                       GameAiSelectionMemory *memory,
                                       GameCommand *out)
{
    GameDomainRef facility_target = none_ref();
    int facility_comparison = 0;
    int previous_group = -1;
    int preferred;
    int ref_index;

    preferred = relation_preferred_player(s, p);
    for (ref_index = 0; ref_index < count; ref_index++) {
        GameDomainRef ref = refs[ref_index];
        if (ref.kind == GAME_DOMAIN_FACILITY) {
            if (ref.id >= 0 && ref.id < s->facility_count &&
                s->facility_owner[ref.id] == p &&
                s->facility_type[ref.id] != 0 &&
                s->facility_type[ref.id] != 4 &&
                s->facility_level[ref.id] >= 3 &&
                s->facility_level[ref.id] > facility_comparison) {
                /* 0x4205F6 stores ESI, not the compared building level. */
                facility_comparison = scan_register;
                facility_target = ref;
                memory->card_target_raw = (uint32_t)(0xfa1 + ref.id);
            }
            continue;
        }
        if (ref.kind != GAME_DOMAIN_LAND || ref.id < 0 ||
            ref.id >= s->land_count) continue;
        {
            int group = s->land_group[ref.id];
            int total = 0;
            int owned = 0;
            int owned_levels = 0;
            int foreign_levels = 0;
            int preferred_owner = 0;
            int i;
            if (group < 0 || group == previous_group)
                continue;
            for (i = 0; i < s->land_count; i++) {
                int owner;
                if (s->land_group[i] != group) continue;
                owner = s->land_owner[i];
                total++;
                if (owner == p) {
                    owned++;
                    if (s->land_level[i] > 0)
                        owned_levels += s->land_level[i];
                } else if (owner >= 0) {
                    if (s->land_level[i] > 0)
                        foreign_levels += s->land_level[i];
                }
                if (preferred >= 0 && owner == preferred) {
                    preferred_owner = 1;
                    break;
                }
            }
            /* ESI is the one-based map scan index, including early exits. */
            scan_register = i + 1;
            previous_group = group;
            if (!preferred_owner && total > 0 &&
                (double)((float)owned / (float)total) >= 0.66 &&
                owned_levels >= 7 && foreign_levels <= 3) {
                *out = rich4_game_domain_command(
                    GAME_COMMAND_USE_CARD, GAME_COMMAND_SOURCE_AI, p,
                    rich4_game_domain_ref(GAME_DOMAIN_CARD, 26),
                    rich4_game_domain_ref(GAME_DOMAIN_LAND, ref.id), 0);
                memory->card_target_raw = (uint32_t)(0x7d1 + ref.id);
                return 1;
            }
        }
    }
    if (facility_comparison != 0 &&
        !rich4_game_domain_ref_is_none(facility_target)) {
        *out = rich4_game_domain_command(
            GAME_COMMAND_USE_CARD, GAME_COMMAND_SOURCE_AI, p,
            rich4_game_domain_ref(GAME_DOMAIN_CARD, 26), facility_target, 0);
        return 1;
    }
    return 0;
}

/* 0x42040E consumes the mixed raw table in order.  Its facility comparison
 * value is the inherited ESI/card cursor or the last land-group scan index;
 * treating this as a maximum-level search changes the original target. */
static int ai_price_increase_command(const GameState *s,
                                     const GameAiGlobalProjection *projection,
                                     int p, int scan_register,
                                     GameAiSelectionMemory *memory, GameCommand *out)
{
    GameDomainRef refs[GAME_MAX_MAP_PROPERTIES * 2];
    int count;
    int node;

    if (s == NULL || out == NULL || p < 0 || p >= s->player_count)
        return 0;
    count = ai_collect_card_global_property_refs(
        s, p, projection, refs, (int)(sizeof(refs) / sizeof(refs[0])));
    if (count >= 0)
        return ai_price_increase_scan_refs(s, p, refs, count, scan_register, memory, out);
    if (s->tool_route_node_count <= 1) return 0;
    count = 0;

    for (node = 1; node < s->tool_route_node_count; node++) {
        uint16_t raw = s->route_object_raw[node];
        if (raw > 0x7d0 && raw < 0xfa0) {
            int id = (int)raw - 0x7d1;
            if (id >= 0 && id < s->land_count && count < (int)(sizeof(refs) /
                                                               sizeof(refs[0])))
                refs[count++] = rich4_game_domain_ref(GAME_DOMAIN_LAND, id);
        } else if (raw > 0xfa0 && raw < 0x1770) {
            int id = (int)raw - 0xfa1;
            if (id >= 0 && id < s->facility_count &&
                count < (int)(sizeof(refs) / sizeof(refs[0])))
                refs[count++] =
                    rich4_game_domain_ref(GAME_DOMAIN_FACILITY, id);
        }
    }
    return ai_price_increase_scan_refs(s, p, refs, count, scan_register, memory, out);
}

/* 0x42062B (original card ID 28) uses the first six forward route objects.
 * The route helper itself may choose a branch with the core RNG.  A land
 * candidate is useful when the other owners' built levels in its name group
 * reach seven; a facility candidate must belong to the relation-preferred
 * player, have a nonzero type, and be built to level three or above. */
static int ai_seal_command(const GameState *s, int p, Rich4Rng *rng,
                           GameCommand *out)
{
    int nodes[8];
    int count;
    int preferred;
    int i;

    if (s == NULL || out == NULL || rng == NULL || p < 0 ||
        p >= s->player_count)
        return 0;
    preferred = relation_preferred_player(s, p);
    count = ai_collect_route_window(s, p, 6, 0, rng, nodes);
    if (count <= 0) return 0;
    for (i = 0; i < count; i++) {
        uint16_t raw = s->route_object_raw[nodes[i]];
        if (raw > 0x7d0 && raw < 0xfa0) {
            int id = (int)raw - 0x7d1;
            int group;
            int foreign_levels = 0;
            int j;
            if (id < 0 || id >= s->land_count) continue;
            group = s->land_group[id];
            if (group < 0) continue;
            for (j = 0; j < s->land_count; j++) {
                if (s->land_group[j] == group && s->land_owner[j] == p)
                    break;
                if (s->land_group[j] == group &&
                    s->land_owner[j] >= 0 &&
                    s->land_owner[j] != p && s->land_level[j] > 0)
                    foreign_levels += s->land_level[j];
            }
            if (j != s->land_count) continue;
            if (foreign_levels >= 7) {
                *out = rich4_game_domain_command(
                    GAME_COMMAND_USE_CARD, GAME_COMMAND_SOURCE_AI, p,
                    rich4_game_domain_ref(GAME_DOMAIN_CARD, 27),
                    rich4_game_domain_ref(GAME_DOMAIN_LAND, id), 0);
                return 1;
            }
        } else if (raw > 0xfa0 && raw < 0x1770) {
            int id = (int)raw - 0xfa1;
            if (id < 0 || id >= s->facility_count ||
                preferred < 0 || s->facility_owner[id] != preferred ||
                s->facility_type[id] == 0 ||
                s->facility_level[id] < 3)
                continue;
            *out = rich4_game_domain_command(
                GAME_COMMAND_USE_CARD, GAME_COMMAND_SOURCE_AI, p,
                rich4_game_domain_ref(GAME_DOMAIN_CARD, 27),
                rich4_game_domain_ref(GAME_DOMAIN_FACILITY, id), 0);
            return 1;
        }
    }
    return 0;
}

static int highest_robbery_card(const GameState *s, int victim,
                                int minimum_f7, int exact_f7)
{
    int best_card = -1, best_price = 0, i;
    for (i = 0; i < GAME_PLAYER_CARD_SLOTS; i++) {
        int original_id = s->players[victim].card_inventory[i];
        const GameCardInfo *info;
        if (original_id <= 0 || original_id > GAME_CARD_COUNT) continue;
        info = rich4_card_info(original_id - 1);
        if (info == NULL ||
            (exact_f7 ? info->ai_difficulty != minimum_f7
                      : info->ai_difficulty < minimum_f7) ||
            info->shop_price <= best_price) continue;
        best_card = original_id - 1;
        best_price = info->shop_price;
    }
    return best_card;
}

static int ai_robbery_command(const GameState *s,
                              const uint8_t *player_on_map,
                              int p, GameCommand *out)
{
    int preferred, best_player = -1, best_card = -1, best_price = 0, i;
    if (player_on_map == NULL) return 0;
    preferred = relation_preferred_player(s, p);
    if (preferred >= 0 && player_on_map[preferred]) {
        int card = highest_robbery_card(s, preferred, 1, 0);
        if (card >= 0) {
            best_player = preferred;
            best_card = card;
        }
    }
    if (best_card < 0) {
        for (i = 0; i < s->player_count; i++) {
            int card, price;
            if (i == p || !player_on_map[i]) continue;
            card = highest_robbery_card(s, i, 2, 1);
            if (card < 0) continue;
            price = rich4_card_catalog[card].shop_price;
            if (price <= best_price) continue;
            best_player = i;
            best_card = card;
            best_price = price;
        }
    }
    if (best_card < 0) return 0;
    *out = rich4_game_domain_command(
        GAME_COMMAND_USE_CARD, GAME_COMMAND_SOURCE_AI, p,
        rich4_game_domain_ref(GAME_DOMAIN_CARD, 12),
        rich4_game_domain_ref(GAME_DOMAIN_PLAYER, best_player), best_card);
    return 1;
}

static int ai_company_owned_at_node(const GameState *s, int owner, int node)
{
    int company;
    if (s == NULL || !s->tool_company_geometry_available ||
        !rich4_tool_valid_node(s, node))
        return 0;
    for (company = 0; company < s->company_count &&
                     company < GAME_MAX_MAP_PROPERTIES; company++) {
        if (s->company_owner[company] == owner &&
            s->tool_company_x[company] == s->tool_route_x[node] &&
            s->tool_company_y[company] == s->tool_route_y[node])
            return 1;
    }
    return 0;
}

/* 0x41FE6F (original card IDs 16 and 17) builds a visible ordinary-player
 * candidate list in raw-table order, skips the current player, sleeping
 * players, and players holding the original card 18 (复仇卡), then prefers
 * the relation target and otherwise chooses uniformly from that list. The
 * original tests only the low four player bits; high-bit special actors are
 * not candidates. */
static int ai_attack_player_command(const GameState *s,
                                    const uint8_t *player_on_map,
                                    const GameAiGlobalProjection *projection,
                                    int p,
                                    int card, Rich4Rng *rng,
                                    GameCommand *out)
{
    int candidates[GAME_MAX_PLAYERS];
    uint8_t player_order[GAME_MAX_PLAYERS];
    int count = 0;
    int player_order_count = 0;
    int preferred;
    int target = -1;
    int i;
    int order_index;

    if (s == NULL || player_on_map == NULL || rng == NULL || out == NULL ||
        p < 0 || p >= s->player_count ||
        (card != 15 && card != 16)) return 0;
    preferred = relation_preferred_player(s, p);
    if (projection != NULL && projection->object_table_available) {
        if (ai_collect_visible_player_order(
                s, projection, player_order, &player_order_count) < 0)
            player_order_count = 0;
    } else {
        for (i = 0; i < s->player_count && i < GAME_MAX_PLAYERS; i++)
            if (player_on_map[i] != 0)
                player_order[player_order_count++] = (uint8_t)i;
    }
    for (order_index = 0; order_index < player_order_count; order_index++) {
        i = player_order[order_index];
        if (i == p || player_on_map[i] == 0 ||
            (s->players[i].play_state & GAME_PLAYER_PLAY_STATE_MASK) == 0 ||
            s->players[i].days_sleeping != 0 ||
            rich4_card_inventory_has(s, i, 17))
            continue;
        candidates[count++] = i;
        if (i == preferred) target = i;
    }
    if (target < 0 && count > 0)
        target = candidates[rich4_rng_original_state_range(rng, count)];
    if (target >= 0) {
        *out = rich4_game_domain_command(
            GAME_COMMAND_USE_CARD, GAME_COMMAND_SOURCE_AI, p,
            rich4_game_domain_ref(GAME_DOMAIN_CARD, card),
            rich4_game_domain_ref(GAME_DOMAIN_PLAYER, target), 0);
        return 1;
    }
    return 0;
}

/* 0x41FACC (original card ID 14) first considers stopping the actor itself
 * on a qualifying owned property. If that direct branch does not apply, the
 * raw table builds a visible-player bitset; the target pass then scans player
 * records in ascending index order and accepts the first ordinary player on
 * one of the actor's qualifying facilities or auxiliary-company records.
 * High-bit special-actor masks are ignored by the original branch. */
static int ai_stay_command(const GameState *s, GameDomainRef current_property,
                           const uint8_t *player_on_map, int p,
                           GameAiSelectionMemory *memory,
                           GameCommand *out)
{
    GameDomainRef property = current_property;
    int64_t cash;
    int i;

    if (s == NULL || out == NULL || p < 0 || p >= s->player_count)
        return 0;
    cash = (int64_t)s->players[p].cash;
    if (rich4_game_domain_ref_is_none(property))
        property = ai_project_current_property(s, p);

    if (s->players[p].days_tortoise_walking == 0 &&
        property.kind == GAME_DOMAIN_LAND && property.id >= 0 &&
        property.id < s->land_count && s->land_owner[property.id] == p &&
        s->land_type[property.id] == 0 &&
        s->land_level[property.id] < 5 &&
        (int64_t)s->land_price_per_level[property.id] * s->price_index < cash &&
        cash + s->players[p].bank > 10000 &&
        s->players[p].god_investment_modifier >= 0) {
        int same_group = 0;
        if (s->land_group[property.id] >= 0) {
            for (i = 0; i < s->land_count; i++) {
                if (i != property.id && s->land_group[i] ==
                        s->land_group[property.id] && s->land_owner[i] == p) {
                    same_group = 1;
                    break;
                }
            }
        }
        if (same_group || s->land_level[property.id] >= 2) {
            int target = -1;
            uint32_t raw;
            if (same_group) memory->card_target_raw = 0x8000u | (1u << p);
            /* 0x41FC0B -> 0x41FBF8 sets success without rewriting 0x48BE58. */
            raw = memory->card_target_raw & 0xffu;
            for (i = 0; i < 8; i++)
                if ((raw & (1u << i)) != 0) { target = i; break; }
            *out = rich4_game_domain_command(
                GAME_COMMAND_USE_CARD, GAME_COMMAND_SOURCE_AI, p,
                rich4_game_domain_ref(GAME_DOMAIN_CARD, 13),
                target < 0 ? none_ref() : rich4_game_domain_ref(
                    target < 4 ? GAME_DOMAIN_PLAYER : GAME_DOMAIN_SPECIAL_PLAYER,
                    target < 4 ? target : target - 4), 0);
            return 1;
        }
    }

    if (s->players[p].days_tortoise_walking == 0 &&
        property.kind == GAME_DOMAIN_FACILITY && property.id >= 0 &&
        property.id < s->facility_count &&
        s->facility_owner[property.id] == p &&
        s->facility_type[property.id] != 0 &&
        s->facility_type[property.id] != 3 &&
        s->facility_level[property.id] < 5 &&
        (int64_t)s->facility_charge_by_level[property.id][0] *
            s->price_index < cash &&
        cash > 10000 && s->players[p].god_investment_modifier >= 0) {
        memory->card_target_raw = 0x8000u | (1u << p);
        *out = rich4_game_domain_command(
            GAME_COMMAND_USE_CARD, GAME_COMMAND_SOURCE_AI, p,
            rich4_game_domain_ref(GAME_DOMAIN_CARD, 13),
            rich4_game_domain_ref(GAME_DOMAIN_PLAYER, p), 0);
        return 1;
    }

    if (player_on_map != NULL) {
        for (i = 0; i < s->player_count && i < GAME_MAX_PLAYERS; i++) {
            uint16_t raw;
            int facility;
            if (i == p || player_on_map[i] == 0 ||
                (s->players[i].play_state &
                 GAME_PLAYER_PLAY_STATE_MASK) == 0 ||
                !rich4_tool_valid_node(s, s->players[i].node)) continue;
            raw = s->route_object_raw[s->players[i].node];
            if (raw > 0xfa0 && raw < 0x1770) {
                facility = (int)raw - 0xfa1;
                if (facility < 0 || facility >= s->facility_count ||
                    s->facility_owner[facility] != p ||
                    s->facility_type[facility] == 0 ||
                    s->facility_level[facility] < 2) continue;
            } else if (raw > 0x1770 && raw < 0x1f40) {
                int company = (int)raw - 0x1771;
                if (company < 0 || company >= s->company_count ||
                    company >= GAME_MAX_MAP_PROPERTIES ||
                    s->company_owner[company] != p) continue;
            } else {
                /* A company-only global-table record is represented by its
                 * typed coordinate rather than a duplicated route raw ID. */
                if (!ai_company_owned_at_node(s, p, s->players[i].node))
                    continue;
            }
            *out = rich4_game_domain_command(
                GAME_COMMAND_USE_CARD, GAME_COMMAND_SOURCE_AI, p,
                rich4_game_domain_ref(GAME_DOMAIN_CARD, 13),
                rich4_game_domain_ref(GAME_DOMAIN_PLAYER, i), 0);
            memory->card_target_raw = 0x8000u | (1u << i);
            return 1;
        }
    }

    return 0;
}

/* 0x41F037 (original card ID 9) scans the raw table for a name-group owned by
 * the current actor with at least three eligible ordinary land records below
 * level five, then chooses one eligible group at random. */
static int ai_angel_command(const GameState *s,
                            const GameAiGlobalProjection *projection,
                            int p, Rich4Rng *rng, GameCommand *out)
{
    int candidate_ids[GAME_MAX_MAP_PROPERTIES];
    int candidate_count = 0;
    GameDomainRef visible[GAME_MAX_MAP_PROPERTIES * 2];
    int visible_count;
    int node;

    if (s == NULL || rng == NULL || out == NULL || p < 0 ||
        p >= s->player_count)
        return 0;
    visible_count = ai_collect_card_global_property_refs(
        s, p, projection, visible,
        (int)(sizeof(visible) / sizeof(visible[0])));
    if (visible_count >= 0) {
        int i;
        for (i = 0; i < visible_count; i++) {
            int id;
            int group;
            int total = 0;
            int seen = 0;
            int j;
            if (visible[i].kind != GAME_DOMAIN_LAND) continue;
            id = visible[i].id;
            if (id < 0 || id >= s->land_count || s->land_owner[id] != p ||
                s->land_type[id] != 0 || s->land_level[id] >= 5)
                continue;
            group = s->land_group[id];
            if (group < 0) continue;
            for (j = 0; j < visible_count; j++) {
                int other_id;
                if (visible[j].kind != GAME_DOMAIN_LAND) continue;
                other_id = visible[j].id;
                if (other_id >= 0 && other_id < s->land_count &&
                    s->land_owner[other_id] == p &&
                    s->land_group[other_id] == group &&
                    s->land_type[other_id] == 0 &&
                    s->land_level[other_id] < 5)
                    total++;
            }
            for (j = 0; j < candidate_count; j++)
                if (s->land_group[candidate_ids[j]] == group) {
                    seen = 1;
                    break;
                }
            if (total >= 3 && !seen &&
                candidate_count < GAME_MAX_MAP_PROPERTIES)
                candidate_ids[candidate_count++] = id;
        }
        if (candidate_count == 0) return 0;
        node = candidate_ids[rich4_rng_original_state_range(rng, candidate_count)];
        *out = rich4_game_domain_command(
            GAME_COMMAND_USE_CARD, GAME_COMMAND_SOURCE_AI, p,
            rich4_game_domain_ref(GAME_DOMAIN_CARD, 8),
            rich4_game_domain_ref(GAME_DOMAIN_LAND, node), 0);
        return 1;
    }
    for (node = 1; node < s->tool_route_node_count; node++) {
        uint16_t raw = s->route_object_raw[node];
        int id;
        int group;
        int total = 0;
        int seen = 0;
        int other_node;
        if (raw <= 0x7d0 || raw >= 0xfa0) continue;
        id = (int)raw - 0x7d1;
        if (id < 0 || id >= s->land_count || s->land_owner[id] != p ||
            s->land_type[id] != 0 || s->land_level[id] >= 5)
            continue;
        group = s->land_group[id];
        if (group < 0) continue;
        for (other_node = 1; other_node < s->tool_route_node_count;
             other_node++) {
            uint16_t other_raw = s->route_object_raw[other_node];
            int other_id;
            if (other_raw <= 0x7d0 || other_raw >= 0xfa0) continue;
            other_id = (int)other_raw - 0x7d1;
            if (other_id < 0 || other_id >= s->land_count ||
                s->land_group[other_id] != group ||
                s->land_type[other_id] != 0 || s->land_level[other_id] >= 5)
                continue;
            total++;
        }
        for (other_node = 0; other_node < candidate_count; other_node++)
            if (s->land_group[candidate_ids[other_node]] == group) {
                seen = 1;
                break;
            }
        if (total >= 3 && !seen)
            candidate_ids[candidate_count++] = id;
    }
    if (candidate_count == 0) return 0;
    node = candidate_ids[rich4_rng_original_state_range(rng, candidate_count)];
    *out = rich4_game_domain_command(
        GAME_COMMAND_USE_CARD, GAME_COMMAND_SOURCE_AI, p,
        rich4_game_domain_ref(GAME_DOMAIN_CARD, 8),
        rich4_game_domain_ref(GAME_DOMAIN_LAND, node), 0);
    return 1;
}

/* 0x41F1B3 (original card ID 10) aggregates ordinary raw-table land by
 * name-group and owner.  A relation-preferred owner qualifies at count >= 2,
 * built-level sum >= 7 while the actor's sum is <= 1; without a relation
 * target the original fallback requires at least three opposing properties
 * and nine opposing built levels. */
static int ai_demon_command(const GameState *s,
                            const GameAiGlobalProjection *projection,
                            int p, GameCommand *out)
{
    int groups[GAME_MAX_MAP_PROPERTIES];
    int representatives[GAME_MAX_MAP_PROPERTIES];
    int counts[GAME_MAX_MAP_PROPERTIES][GAME_MAX_PLAYERS];
    int levels[GAME_MAX_MAP_PROPERTIES][GAME_MAX_PLAYERS];
    GameDomainRef visible[GAME_MAX_MAP_PROPERTIES * 2];
    int group_count = 0;
    int visible_count;
    int ref_count;
    int preferred;
    int node;
    int i;

    if (s == NULL || out == NULL || p < 0 || p >= s->player_count)
        return 0;
    memset(counts, 0, sizeof(counts));
    memset(levels, 0, sizeof(levels));
    preferred = relation_preferred_player(s, p);
    visible_count = ai_collect_card_global_property_refs(
        s, p, projection, visible,
        (int)(sizeof(visible) / sizeof(visible[0])));
    ref_count = visible_count;
    if (visible_count < 0) {
        if (s->tool_route_node_count <= 1) return 0;
        ref_count = 0;
        for (node = 1; node < s->tool_route_node_count; node++) {
            uint16_t raw = s->route_object_raw[node];
            int id;
            if (raw <= 0x7d0 || raw >= 0xfa0) continue;
            id = (int)raw - 0x7d1;
            if (id < 0 || id >= s->land_count ||
                ref_count >= (int)(sizeof(visible) / sizeof(visible[0])))
                continue;
            visible[ref_count++] = rich4_game_domain_ref(
                GAME_DOMAIN_LAND, id);
        }
    }
    for (i = 0; i < ref_count; i++) {
        GameDomainRef ref = visible[i];
        int id;
        int group;
        int index = -1;
        int owner;
        int g;
        if (ref.kind != GAME_DOMAIN_LAND) continue;
        id = ref.id;
        if (id < 0 || id >= s->land_count || s->land_type[id] != 0)
            continue;
        owner = s->land_owner[id];
        if (owner < 0 || owner >= s->player_count || owner >= GAME_MAX_PLAYERS)
            continue;
        group = s->land_group[id];
        if (group < 0) continue;
        for (g = 0; g < group_count; g++)
            if (groups[g] == group) {
                index = g;
                break;
            }
        if (index < 0) {
            if (group_count >= GAME_MAX_MAP_PROPERTIES) continue;
            index = group_count++;
            groups[index] = group;
            representatives[index] = id;
        }
        counts[index][owner]++;
        if (s->land_level[id] > 0)
            levels[index][owner] += s->land_level[id];
    }
    for (i = 0; i < group_count; i++) {
        int opposing_count = 0;
        int opposing_levels = 0;
        int owner;
        if (preferred >= 0) {
            if (counts[i][preferred] < 2 || levels[i][preferred] < 7 ||
                levels[i][p] > 1)
                continue;
        } else {
            if (levels[i][p] != 0) continue;
            for (owner = 0; owner < s->player_count &&
                               owner < GAME_MAX_PLAYERS; owner++) {
                if (owner == p ||
                    (s->players[owner].play_state &
                     GAME_PLAYER_PLAY_STATE_MASK) == 0)
                    continue;
                opposing_count += counts[i][owner];
                opposing_levels += levels[i][owner];
            }
            if (opposing_count < 3 || opposing_levels < 9) continue;
        }
        *out = rich4_game_domain_command(
            GAME_COMMAND_USE_CARD, GAME_COMMAND_SOURCE_AI, p,
            rich4_game_domain_ref(GAME_DOMAIN_CARD, 9),
            rich4_game_domain_ref(GAME_DOMAIN_LAND, representatives[i]), 0);
        return 1;
    }
    return 0;
}

/* 0x420055 (original card ID 24) chooses the held stock with the greatest
 * cost basis, while rejecting a closed market, suspended stocks and the
 * original one-sided limit-up state.  The original compares the truncated
 * float product before applying those filters; preserve that ordering so a
 * rejected high-value candidate does not silently change the later scan's
 * threshold. */
static int ai_red_stock_command(const GameState *s, int p, GameCommand *out,
                                GameAiCandidateResult *reason)
{
    int best_value = 0;
    int best = -1;
    int i;
    if (rich4_stock_market_is_closed(s)) {
        *reason = GAME_AI_CANDIDATE_MARKET_CLOSED;
        return 0;
    }
    for (i = 0; i < GAME_STOCK_COUNT; i++) {
        double raw = (double)s->player_stock_amount[p][i] *
                     s->player_stock_average_cost[p][i];
        int value;
        if (raw <= (double)best_value || raw > (double)INT_MAX)
            continue;
        value = (int)raw;
        if (value <= best_value || s->stocks[i].f6 != 0 ||
            rich4_stock_price_state(s->stocks[i].f16, s->stocks[i].f20) ==
                GAME_STOCK_PRICE_LIMIT_UP)
            continue;
        best_value = value;
        best = i;
    }
    if (best < 0) {
        *reason = GAME_AI_CANDIDATE_RULE_REJECTED;
        return 0;
    }
    *out = rich4_game_domain_command(
        GAME_COMMAND_USE_CARD, GAME_COMMAND_SOURCE_AI, p,
        rich4_game_domain_ref(GAME_DOMAIN_CARD, 23),
        rich4_game_domain_ref(GAME_DOMAIN_STOCK, best), 0);
    return 1;
}

/* 0x4200EA (original card ID 25) first scans the player with the most
 * auxiliary-company owners, then scans the relation-preferred player.  The
 * original keeps both possible stock targets and gives the relation scan
 * priority when it found one; the company target is only the fallback.  In
 * both scans the raw cost-basis comparison happens before the eligibility
 * filters, so a rejected high-value stock still raises the later threshold.
 * Its price-state helper rejects only the original result 3, i.e. the
 * one-sided limit-down state (not an ordinary falling quote). */
static int ai_black_stock_command(const GameState *s, int p, GameCommand *out,
                                  GameAiCandidateResult *reason)
{
    int company_player = -1;
    int company_best_count = 0;
    int company_stock = -1;
    int relation_stock = -1;
    int i;

    if (rich4_stock_market_is_closed(s)) {
        *reason = GAME_AI_CANDIDATE_MARKET_CLOSED;
        return 0;
    }

    for (i = 0; i < s->player_count; i++) {
        int count = 0;
        int j;
        if (i == p) continue;
        for (j = 0; j < s->company_count; j++)
            if (s->company_owner[j] == i) count++;
        if (count > company_best_count) {
            company_best_count = count;
            company_player = i;
        }
    }

    if (company_player >= 0) {
        int best_value = 0;
        for (i = 0; i < GAME_STOCK_COUNT; i++) {
            double raw = (double)s->player_stock_amount[company_player][i] *
                         s->player_stock_average_cost[company_player][i];
            int value;
            if (raw <= (double)best_value || raw > (double)INT_MAX)
                continue;
            value = (int)raw;
            if (value <= best_value || s->stocks[i].f4 == 0 ||
                s->stocks[i].f6 != 0 ||
                s->player_stock_amount[p][i] != 0 ||
                rich4_stock_price_state(s->stocks[i].f16, s->stocks[i].f20) ==
                    GAME_STOCK_PRICE_LIMIT_DOWN)
                continue;
            best_value = value;
            company_stock = i;
        }
    }

    i = relation_preferred_player(s, p);
    if (i >= 0) {
        int best_value = 0;
        for (int stock = 0; stock < GAME_STOCK_COUNT; stock++) {
            double raw = (double)s->player_stock_amount[i][stock] *
                         s->player_stock_average_cost[i][stock];
            int value;
            if (raw <= (double)best_value || raw > (double)INT_MAX)
                continue;
            value = (int)raw;
            if (value <= best_value || s->stocks[stock].f6 != 0 ||
                s->player_stock_amount[p][stock] != 0 ||
                rich4_stock_price_state(s->stocks[stock].f16,
                                        s->stocks[stock].f20) ==
                    GAME_STOCK_PRICE_LIMIT_DOWN)
                continue;
            best_value = value;
            relation_stock = stock;
        }
    }

    if (relation_stock < 0) relation_stock = company_stock;
    if (relation_stock < 0) {
        *reason = GAME_AI_CANDIDATE_RULE_REJECTED;
        return 0;
    }
    *out = rich4_game_domain_command(
        GAME_COMMAND_USE_CARD, GAME_COMMAND_SOURCE_AI, p,
        rich4_game_domain_ref(GAME_DOMAIN_CARD, 24),
        rich4_game_domain_ref(GAME_DOMAIN_STOCK, relation_stock), 0);
    return 1;
}

static int ai_nearest_god_command(
    const GameState *s, const GameAiMapTargetContext *target_context,
    int p, GameCommand *out, GameAiCandidateResult *reason)
{
    int attached_slot = s->players[p].god_info;
    int selected_slot;
    if (attached_slot == 1 || attached_slot == 2 || attached_slot == 3 ||
        attached_slot == 4 || attached_slot == 12) {
        *reason = GAME_AI_CANDIDATE_RULE_REJECTED;
        return 0;
    }
    if (target_context == NULL ||
        target_context->nearest_dynamic_god.kind != GAME_DOMAIN_GOD ||
        target_context->nearest_dynamic_god.id < 0 ||
        target_context->nearest_dynamic_god.id >=
            GAME_MAP_DYNAMIC_OBJECT_COUNT ||
        !rich4_god_is_road_type(target_context->nearest_dynamic_god_type)) {
        *reason = GAME_AI_CANDIDATE_DEFERRED_TARGET;
        return 0;
    }
    selected_slot = target_context->nearest_dynamic_god.id + 1;
    if (selected_slot != 1 && selected_slot != 2 && selected_slot != 3 &&
        selected_slot != 4 && selected_slot != 12) {
        *reason = GAME_AI_CANDIDATE_RULE_REJECTED;
        return 0;
    }
    *out = rich4_game_domain_command(
        GAME_COMMAND_USE_CARD, GAME_COMMAND_SOURCE_AI, p,
        rich4_game_domain_ref(GAME_DOMAIN_CARD, 22),
        target_context->nearest_dynamic_god,
        target_context->nearest_dynamic_god_type);
    if (rich4_card_inspect(s, out) != GAME_CARD_USE_APPLIED) {
        *reason = GAME_AI_CANDIDATE_RULE_REJECTED;
        return 0;
    }
    return 1;
}

/* 0x41FF77 (original card ID 22) is a no-target eligibility helper, not a
 * generic no-target card.  The original checks a sendable attached god first;
 * only when no god is attached does it accept an attached timed bomb whose
 * countdown is below thirteen.  Keep the zero-based host slot separate from
 * the original one-based player-record byte. */
static int ai_send_god_eligible(const GameState *s, int p)
{
    const GamePlayerState *player;
    int slot;

    if (s == NULL || p < 0 || p >= s->player_count) return 0;
    player = &s->players[p];
    if (player->god_info != 0)
        return rich4_god_is_sendable_type(rich4_god_player_type(player));
    slot = player->attached_timed_bomb_slot;
    return slot >= 0 && slot < GAME_TOOL_PLACED_OBJECT_COUNT &&
           s->tool_placed_objects[slot].kind == 0x12 &&
           s->tool_placed_objects[slot].attached_player == p + 1 &&
           s->tool_placed_objects[slot].countdown < 13;
}

static int card_command(const GameState *s, GameDomainRef current_property,
                        const uint8_t *player_on_map,
                        const GameAiMapTargetContext *target_context,
                        const GameAiGlobalProjection *projection,
                        GameAiSelectionMemory *memory,
                        int p, int card, int card_cursor,
                        Rich4Rng *rng, GameCommand *out,
                        GameAiCandidateResult *reason)
{
    const GameCardInfo *info = rich4_card_info(card);
    int64_t sum = 0;
    int active = 0;
    int i;
    if (info == NULL) { *reason = GAME_AI_CANDIDATE_RULE_REJECTED; return 0; }
    if (card == 3) {
        if (!ai_swap_land_command(
                s, projection, current_property, p, out)) {
            *reason = rich4_game_domain_ref_is_none(current_property)
                ? GAME_AI_CANDIDATE_DEFERRED_TARGET
                : GAME_AI_CANDIDATE_RULE_REJECTED;
            return 0;
        }
        return 1;
    }
    if (card == 6) {
        if (!ai_rebuild_command(s, current_property, p, rng, out)) {
            *reason = player_on_map == NULL ? GAME_AI_CANDIDATE_DEFERRED_TARGET :
                                               GAME_AI_CANDIDATE_RULE_REJECTED;
            return 0;
        }
        return 1;
    }
    if (card == 2) {
        if (!ai_purchase_command(s, current_property, p, out)) {
            *reason = player_on_map == NULL ? GAME_AI_CANDIDATE_DEFERRED_TARGET :
                                               GAME_AI_CANDIDATE_RULE_REJECTED;
            return 0;
        }
        return 1;
    }
    if (card == 7) {
        if (!ai_auction_command(s, current_property, p, out)) {
            *reason = player_on_map == NULL ? GAME_AI_CANDIDATE_DEFERRED_TARGET :
                                               GAME_AI_CANDIDATE_RULE_REJECTED;
            return 0;
        }
        return 1;
    }
    if (card == 1) {
        if (!ai_average_poverty_command(s, player_on_map, p, out)) {
            *reason = player_on_map == NULL ? GAME_AI_CANDIDATE_DEFERRED_TARGET :
                                               GAME_AI_CANDIDATE_RULE_REJECTED;
            return 0;
        }
        return 1;
    }
    if (card == 10) {
        if (!ai_monster_command(s, projection, p, out)) {
            *reason = GAME_AI_CANDIDATE_RULE_REJECTED;
            return 0;
        }
        return 1;
    }
    if (card == 11) {
        if (!ai_demolish_command(s, projection, p, out)) {
            *reason = GAME_AI_CANDIDATE_RULE_REJECTED;
            return 0;
        }
        return 1;
    }
    if (card == 12) {
        if (!ai_robbery_command(s, player_on_map, p, out)) {
            *reason = player_on_map == NULL ? GAME_AI_CANDIDATE_DEFERRED_TARGET :
                                               GAME_AI_CANDIDATE_RULE_REJECTED;
            return 0;
        }
        return 1;
    }
    if (card == 15 || card == 16) {
        if (!ai_attack_player_command(
                s, player_on_map, projection, p, card, rng, out)) {
            *reason = player_on_map == NULL ? GAME_AI_CANDIDATE_DEFERRED_TARGET :
                                               GAME_AI_CANDIDATE_RULE_REJECTED;
            return 0;
        }
        return 1;
    }
    if (card == 13) {
        if (!ai_stay_command(s, current_property, player_on_map, p, memory, out)) {
            *reason = player_on_map == NULL ? GAME_AI_CANDIDATE_DEFERRED_TARGET :
                                               GAME_AI_CANDIDATE_RULE_REJECTED;
            return 0;
        }
        return 1;
    }
    if (card == 8) {
        if (!ai_angel_command(s, projection, p, rng, out)) {
            *reason = s->tool_route_node_count <= 1 ?
                GAME_AI_CANDIDATE_DEFERRED_TARGET :
                GAME_AI_CANDIDATE_RULE_REJECTED;
            return 0;
        }
        return 1;
    }
    if (card == 9) {
        if (!ai_demon_command(s, projection, p, out)) {
            *reason = s->tool_route_node_count <= 1 ?
                GAME_AI_CANDIDATE_DEFERRED_TARGET :
                GAME_AI_CANDIDATE_RULE_REJECTED;
            return 0;
        }
        return 1;
    }
    if (card == 23) {
        if (!ai_red_stock_command(s, p, out, reason)) return 0;
        return 1;
    }
    if (card == 24) {
        if (!ai_black_stock_command(s, p, out, reason)) return 0;
        return 1;
    }
    if (card == 22) {
        return ai_nearest_god_command(s, target_context, p, out, reason);
    }
    if (card == 25) {
        if (!ai_tax_command(s, player_on_map, p, out)) {
            *reason = player_on_map == NULL ? GAME_AI_CANDIDATE_DEFERRED_TARGET :
                                               GAME_AI_CANDIDATE_RULE_REJECTED;
            return 0;
        }
        return 1;
    }
    if (card == 26) {
        if (!ai_price_increase_command(s, projection, p, card_cursor, memory, out)) {
            *reason = s->tool_route_node_count <= 1 ?
                GAME_AI_CANDIDATE_DEFERRED_TARGET :
                GAME_AI_CANDIDATE_RULE_REJECTED;
            return 0;
        }
        return 1;
    }
    if (card == 27) {
        if (!ai_seal_command(s, p, rng, out)) {
            *reason = s->tool_route_node_count <= 1 ?
                GAME_AI_CANDIDATE_DEFERRED_TARGET :
                GAME_AI_CANDIDATE_RULE_REJECTED;
            return 0;
        }
        return 1;
    }
    if (card == 28) {
        if (!ai_alliance_command(s, player_on_map, p, out)) {
            *reason = player_on_map == NULL ? GAME_AI_CANDIDATE_DEFERRED_TARGET :
                                               GAME_AI_CANDIDATE_RULE_REJECTED;
            return 0;
        }
        return 1;
    }
    if (card == 29) {
        int target = -1;
        int gate = ai_card30_direct_gate(s, p, rng, projection, &target);
        if (gate <= 0) {
            *reason = gate < 0 ? GAME_AI_CANDIDATE_DEFERRED_TARGET :
                                GAME_AI_CANDIDATE_RULE_REJECTED;
            return 0;
        }
        *out = rich4_game_domain_command(
            GAME_COMMAND_USE_CARD, GAME_COMMAND_SOURCE_AI, p,
            rich4_game_domain_ref(GAME_DOMAIN_CARD, 29),
            rich4_game_domain_ref(GAME_DOMAIN_PLAYER,
                                  target >= 0 ? target : p), 0);
        if (rich4_card_inspect(s, out) != GAME_CARD_USE_APPLIED) {
            *reason = GAME_AI_CANDIDATE_RULE_REJECTED;
            return 0;
        }
        return 1;
    }
    if (card == 21) {
        if (!ai_send_god_eligible(s, p)) {
            *reason = GAME_AI_CANDIDATE_RULE_REJECTED;
            return 0;
        }
        *out = rich4_game_domain_command(
            GAME_COMMAND_USE_CARD, GAME_COMMAND_SOURCE_AI, p,
            rich4_game_domain_ref(GAME_DOMAIN_CARD, 21), none_ref(), 0);
        if (rich4_card_inspect(s, out) != GAME_CARD_USE_APPLIED) {
            *reason = GAME_AI_CANDIDATE_RULE_REJECTED;
            return 0;
        }
        return 1;
    }
    if (card == 0) {
        int average;
        for (i = 0; i < s->player_count; i++) {
            if ((s->players[i].play_state & GAME_PLAYER_PLAY_STATE_MASK) == 0)
                continue;
            sum += s->players[i].cash;
            active++;
        }
        if (active == 0 || sum < 0 || sum > (int64_t)INT_MAX * active) {
            *reason = GAME_AI_CANDIDATE_RULE_REJECTED;
            return 0;
        }
        average = (int)(sum / active);
        /* 0x41E6FE: use only when the actor is dramatically poorer than
         * the active-player average and below the price-index cash ceiling. */
        if ((int64_t)average <= (int64_t)s->players[p].cash * 10 ||
            (int64_t)s->players[p].cash >= (int64_t)3000 * s->price_index) {
            *reason = GAME_AI_CANDIDATE_NO_SCORE;
            return 0;
        }
    }
    if (card == 14 && rich4_rng_original_state_range(rng, 4) != 0) {
        /* 0x41FE4E (original card 15): the winter-sleep card's AI helper is
         * only rand()%4==0; its no-target handler is checked below. */
        *reason = GAME_AI_CANDIDATE_RANDOM_GATE;
        return 0;
    }
    if (card == 4 || card == 5 || (card >= 17 && card <= 20)) {
        /* The original AI table entries for card IDs 5 and 6 both point at
         * 0x41E6E3, which is an intentional no-op (xor eax,eax; ret).
         * Passive card IDs 18..21 share the same eligibility no-op. They
         * must leave the inventory scan free to try the following card. */
        *reason = GAME_AI_CANDIDATE_RULE_REJECTED;
        return 0;
    }
    if (info->target_kind != GAME_CARD_TARGET_NONE) {
        *reason = GAME_AI_CANDIDATE_DEFERRED_TARGET;
        return 0;
    }
    *out = rich4_game_domain_command(GAME_COMMAND_USE_CARD,
        GAME_COMMAND_SOURCE_AI, p, rich4_game_domain_ref(GAME_DOMAIN_CARD, card),
        none_ref(), 0);
    if (rich4_card_inspect(s, out) != GAME_CARD_USE_APPLIED) {
        *reason = GAME_AI_CANDIDATE_RULE_REJECTED;
        return 0;
    }
    return 1;
}

static void remember_card_target(GameAiSelectionMemory *memory,
                                  const GameCommand *command)
{
    int card = command->subject.id;
    GameDomainRef target = command->target;
    /* These eligibility entries do not write 0x48BE58. Stay records its
     * writes inside ai_stay_command to preserve its success-only branch. */
    if (card == 0 || card == 2 || card == 7 || card == 13 || card == 14 ||
        card == 21) return;
    if (card == 6) {
        if (target.kind == GAME_DOMAIN_FACILITY)
            memory->card_target_raw = (uint32_t)command->value2;
        return;
    }
    if (target.kind == GAME_DOMAIN_PLAYER && target.id >= 0 && target.id < 4)
        memory->card_target_raw = 0x8000u | (1u << target.id);
    else if (target.kind == GAME_DOMAIN_LAND)
        memory->card_target_raw = (uint32_t)(0x7d1 + target.id);
    else if (target.kind == GAME_DOMAIN_FACILITY)
        memory->card_target_raw = (uint32_t)(0xfa1 + target.id);
    else if (target.kind == GAME_DOMAIN_STOCK)
        memory->card_target_raw = (uint32_t)target.id;
    else if (target.kind == GAME_DOMAIN_GOD)
        memory->card_target_raw = (uint32_t)(target.id + 1);
    else if (target.kind == GAME_DOMAIN_MAP_OBJECT)
        memory->card_target_raw = 0x8000u |
            ((uint32_t)(target.id + 17) << 8);
}

static void select_card(const GameState *s, GameDomainRef current_property,
                        const uint8_t *player_on_map,
                        const GameAiMapTargetContext *target_context,
                        const GameAiGlobalProjection *projection,
                        int p, Rich4Rng *rng, GameAiDecision *d)
{
    int count, start, checked;
    if ((s->players[p].ai_action_flags & 1) == 0) {
        trace(d, none_ref(), none_ref(), GAME_AI_CANDIDATE_DISABLED, 0, 0);
        return;
    }
    count = rich4_card_inventory_count(s, p);
    if (!count) {
        trace(d, none_ref(), none_ref(), GAME_AI_CANDIDATE_NO_INVENTORY, 0, 0);
        return;
    }
    start = count > 8 ? rich4_rng_original_state_range(rng, count) : 0;
    for (checked = 0; checked < 8 && checked < count; checked++) {
        int index = count > 8 ? (start + checked) % count : checked;
        int card = (int)s->players[p].card_inventory[index] - 1;
        if (card < 0) return; /* 0x441DA9 stops at a raw empty slot. */
        const GameCardInfo *info = rich4_card_info(card);
        GameAiCandidateResult reason = GAME_AI_CANDIDATE_RULE_REJECTED;
        GameCommand command;
        memset(&command, 0, sizeof(command));
        GameDomainRef ref = rich4_game_domain_ref(GAME_DOMAIN_CARD, card);
        if (info == NULL || !personality_gate(info->ai_difficulty,
                s->players[p].ai_personality, rng, &reason)) {
            trace(d, ref, none_ref(), reason, 0, 0);
            continue;
        }
        if (!card_command(s, current_property, player_on_map, target_context,
                          projection, &d->selection_after, p, card,
                          count > 8 ? (start + 8) % count : 8, rng,
                          &command, &reason)) {
            trace(d, ref, none_ref(), reason, 0, 0);
            if (command.type == GAME_COMMAND_USE_CARD) {
                remember_card_target(&d->selection_after, &command);
                return;
            }
            continue;
        }
        d->command = command;
        d->has_command = 1;
        remember_card_target(&d->selection_after, &command);
        trace(d, ref, command.target, GAME_AI_CANDIDATE_SELECTED, 0, 0);
        return;
    }
}

static int ai_robot_command(const GameState *s, int p, Rich4Rng *rng,
                            const GameAiGlobalProjection *projection,
                            GameCommand *out, GameAiCandidateResult *reason)
{
    static const unsigned char facility_max_level[5] = {1, 5, 5, 1, 5};
    AiGlobalProperty global[GAME_MAX_MAP_PROPERTIES * 2];
    int global_count;
    int best_kind = GAME_DOMAIN_NONE;
    int best_id = -1;
    int best_score = 0;
    int i;

    global_count = ai_collect_global_properties(
        s, projection, global, (int)(sizeof(global) / sizeof(global[0])));
    if (global_count >= 0) {
        for (i = 0; i < global_count; i++) {
            GameDomainRef ref = global[i].ref;
            int level;
            int score;
            if (ref.kind == GAME_DOMAIN_LAND) {
                level = s->land_level[ref.id];
                /* 0x421BA6 checks the land subtype, not the parity overlay. */
                if (s->land_owner[ref.id] != p ||
                    s->land_type[ref.id] != 0 || level < 0 || level >= 5)
                    continue;
                score = s->land_toll_by_level[ref.id][level];
            } else {
                int type = s->facility_type[ref.id];
                level = s->facility_level[ref.id];
                if (type < 0 || type >= 5 ||
                    s->facility_owner[ref.id] != p || level < 0 ||
                    level >= facility_max_level[type])
                    continue;
                /* The original indexes the facility charge table at the
                 * current level, including level zero. */
                score = s->facility_charge_by_level[ref.id][level];
            }
            if (score > best_score) {
                best_kind = ref.kind;
                best_id = ref.id;
                best_score = score;
            }
        }
    } else {
        /* Preserve the typed fallback for unit states that have no map
         * coordinates.  It is still a complete property set, but it has no
         * original row-major order to reproduce. */
        for (i = 0; i < s->land_count; i++) {
            int level = s->land_level[i];
            int score;
            if (s->land_owner[i] != p || s->land_type[i] != 0 ||
                level < 0 || level >= 5) continue;
            score = s->land_toll_by_level[i][level];
            if (score > best_score) {
                best_kind = GAME_DOMAIN_LAND;
                best_id = i;
                best_score = score;
            }
        }
        for (i = 0; i < s->facility_count; i++) {
            int level = s->facility_level[i];
            int type = s->facility_type[i];
            int score;
            if (type < 0 || type >= 5 || s->facility_owner[i] != p ||
                level < 0 || level >= facility_max_level[type])
                continue;
            score = s->facility_charge_by_level[i][level];
            if (score > best_score) {
                best_kind = GAME_DOMAIN_FACILITY;
                best_id = i;
                best_score = score;
            }
        }
    }
    if (best_id < 0) {
        *reason = GAME_AI_CANDIDATE_RULE_REJECTED;
        return 0;
    }
    *out = rich4_game_domain_command(
        GAME_COMMAND_USE_TOOL, GAME_COMMAND_SOURCE_AI, p,
        rich4_game_domain_ref(GAME_DOMAIN_TOOL, 8),
        rich4_game_domain_ref(best_kind, best_id), 0);
    if (best_kind == GAME_DOMAIN_FACILITY && s->facility_level[best_id] == 0)
        out->value = 1 + rich4_rng_original_state_range(rng, 4);
    if (rich4_tool_inspect(s, out) != GAME_TOOL_USE_APPLIED) {
        *reason = GAME_AI_CANDIDATE_RULE_REJECTED;
        return 0;
    }
    return 1;
}

/* 0x40B221/0x40B343 build the short route window used by the original AI
 * target handlers.  Keep the map walk in the pure selector: route geometry
 * and the immutable route flags are already projected into GameState, while
 * the presentation layer remains uninvolved. */
static int ai_collect_route_window(const GameState *s, int p, int limit,
                                   int reverse, Rich4Rng *rng,
                                   int out_nodes[8])
{
    int current;
    int previous;
    int count = 0;

    if (s == NULL || rng == NULL || out_nodes == NULL ||
        p < 0 || p >= s->player_count || limit <= 0)
        return 0;
    if (limit > 8) limit = 8;
    current = reverse ? s->players[p].previous_node : s->players[p].node;
    previous = reverse ? s->players[p].node : s->players[p].previous_node;
    if (!rich4_tool_valid_node(s, current)) {
        /* During the first preaction there is no previous node yet.  The
         * original reverse helper falls through to its global candidate
         * table; using the live node keeps this port useful before that
         * table is projected without inventing a target. */
        if (!reverse) return 0;
        current = s->players[p].node;
        previous = s->players[p].previous_node;
    }
    while (count < limit && rich4_tool_valid_node(s, current)) {
        int options[4];
        int option_count = 0;
        int edge;
        int selected;
        for (edge = 0; edge < 4; edge++) {
            int next = s->tool_route_next[current][edge];
            int duplicate = 0;
            int i;
            if (!rich4_tool_valid_node(s, next) || next == previous ||
                (s->tool_route_edge_blocked[current] & (1u << edge)) != 0)
                continue;
            for (i = 0; i < option_count; i++)
                if (options[i] == next) duplicate = 1;
            if (!duplicate) options[option_count++] = next;
        }
        if (option_count <= 0) break;
        selected = option_count == 1 ? options[0] :
            options[rich4_rng_original_state_range(rng, option_count)];
        out_nodes[count++] = selected;
        previous = current;
        current = selected;
    }
    return count;
}

/* Exact route-array walk shared by 0x40B221 and 0x40B343. The caller selects
 * the starting/current pair: 0x40B221 starts at the actor node and walks away
 * from its previous node; 0x40B343 starts at the previous node (the prechosen
 * next node before the first move) and walks away from the actor node. Unlike
 * the general route helper, the original keeps duplicate edges, records
 * whether any fork occurred, and repeats the previous node at a dead end. */
static int ai_collect_original_route_window(
    const GameState *s, int start_node, int previous_node, int limit,
    Rich4Rng *rng, int out_nodes[8], int *out_had_branch)
{
    int current;
    int previous;
    int count;

    if (out_had_branch != NULL) *out_had_branch = 0;
    if (s == NULL || rng == NULL || out_nodes == NULL || limit <= 0 ||
        limit > 8 || s->tool_route_node_count <= 1 ||
        s->tool_route_node_count > GAME_MAX_MAP_ROUTE_NODES)
        return 0;
    current = start_node;
    previous = previous_node;
    if (current < 0 || current >= s->tool_route_node_count ||
        previous < 0 || previous >= s->tool_route_node_count) return 0;
    for (count = 0; count < limit; count++) {
        int options[4];
        int option_count = 0;
        int edge;
        int selected;
        for (edge = 0; edge < 4; edge++) {
            int next = s->tool_route_next[current][edge];
            if (next == 0 || next == previous ||
                next >= s->tool_route_node_count ||
                (s->tool_route_edge_blocked[current] & (1u << edge)) != 0)
                continue;
            options[option_count++] = next;
        }
        if (option_count > 1 && out_had_branch != NULL)
            *out_had_branch = 1;
        selected = option_count == 0 ? previous :
            options[option_count == 1 ? 0 :
                    rich4_rng_original_state_range(rng, option_count)];
        out_nodes[count] = selected;
        previous = current;
        current = selected;
    }
    return count;
}

static int ai_route_window_contains(const int *nodes, int count, int node)
{
    int i;
    for (i = 0; i < count; i++)
        if (nodes[i] == node) return 1;
    return 0;
}

/* The mine and timed-bomb handlers intersect 0x40B343's six-node route
 * window with 0x409EF9's visible route list when a viewport is available.
 * The typed core-only fallback keeps the same path and candidate predicates
 * but cannot reproduce visibility or pixel-collision order. */
static int ai_route_object_tool_command(
    const GameState *s, int p, int tool, Rich4Rng *rng,
    const GameAiGlobalProjection *projection, GameCommand *out,
    GameAiCandidateResult *reason)
{
    int path_nodes[8];
    int path_count;
    int visible_nodes[GAME_MAX_MAP_ROUTE_NODES];
    int visible_count;
    int candidates[GAME_MAX_MAP_ROUTE_NODES];
    int candidate_count = 0;
    int confinement_node = -1;
    int selected;
    int i;

    int prison_occupied = 0;
    int hospital_occupied = 0;

    for (i = 0; i < GAME_MAX_PLAYERS && i < 4; i++) {
        prison_occupied |= s->players[i].confinement_actor_type ==
            GAME_CONFINEMENT_PRISON;
        hospital_occupied |= s->players[i].confinement_actor_type ==
            GAME_CONFINEMENT_HOSPITAL;
    }
    {
        int start_node = s->players[p].previous_node;
        int previous_node = s->players[p].node;
        /* At the initial AI preaction, the original +6 field still contains
         * the first preselected neighbor; GameState stores that edge in
         * next_node while previous_node remains zero. */
        if (start_node == 0) start_node = s->players[p].next_node;
        path_count = ai_collect_original_route_window(
            s, start_node, previous_node, 6, rng, path_nodes, NULL);
    }
    if (path_count <= 0) {
        *reason = GAME_AI_CANDIDATE_DEFERRED_TARGET;
        return 0;
    }
    if (projection != NULL) {
        visible_count = ai_collect_global_route_nodes(
            s, projection, visible_nodes, GAME_MAX_MAP_ROUTE_NODES);
        if (visible_count < 0) {
            *reason = GAME_AI_CANDIDATE_DEFERRED_TARGET;
            return 0;
        }
    } else {
        visible_count = path_count;
        for (i = 0; i < path_count; i++) visible_nodes[i] = path_nodes[i];
    }
    for (i = 0; i < visible_count; i++) {
        int node = visible_nodes[i];
        uint16_t raw;
        int owner = -1;
        if (!ai_route_window_contains(path_nodes, path_count, node))
            continue;
        if (ai_route_has_occupant(s, projection, node, 0x00ffff00u))
            continue;
        if ((node == s->prison_node && prison_occupied) ||
            (node == s->hospital_node && hospital_occupied)) {
            confinement_node = node;
            break;
        }
        if (tool == 3) {
            candidates[candidate_count++] = node;
            continue;
        }
        raw = s->route_object_raw[node];
        if (raw > 2000u && raw < 4000u) {
            int id = (int)raw - 2001;
            if (id >= 0 && id < s->land_count)
                owner = s->land_owner[id];
        } else if (raw > 4000u && raw < 6000u) {
            int id = (int)raw - 4001;
            if (id >= 0 && id < s->facility_count)
                owner = s->facility_owner[id];
        }
        if (owner >= 0 && owner != p)
            candidates[candidate_count++] = node;
    }
    if (confinement_node > 0) {
        selected = confinement_node;
    } else if (candidate_count > 0) {
        selected = candidates[rich4_rng_original_state_range(rng, candidate_count)];
    } else {
        *reason = GAME_AI_CANDIDATE_RULE_REJECTED;
        return 0;
    }
    *out = rich4_game_domain_command(
        GAME_COMMAND_USE_TOOL, GAME_COMMAND_SOURCE_AI, p,
        rich4_game_domain_ref(GAME_DOMAIN_TOOL, tool),
        rich4_game_domain_ref(GAME_DOMAIN_ROUTE_NODE, selected), 0);
    if (rich4_tool_inspect(s, out) == GAME_TOOL_USE_APPLIED)
        return 1;
    *reason = GAME_AI_CANDIDATE_RULE_REJECTED;
    return 0;
}

static int ai_roadblock_land_name_group(
    const GameState *s, const GameAiGlobalProjection *projection, int id)
{
    if (s == NULL || id < 0 || id >= s->land_count) return -1;
    if (projection != NULL && projection->land_name_groups != NULL) {
        if (id >= projection->land_name_group_count) return -1;
        return projection->land_name_groups[id];
    }
    /* Core-only states retain their proven ordinary-land name groups. */
    if (s->land_type[id] == 0) return s->land_group[id];
    return -1;
}

static int64_t ai_roadblock_land_group_value(
    const GameState *s, const GameAiGlobalProjection *projection,
    int player, int group)
{
    int64_t value = 0;
    int i;
    if (s == NULL || player < 0 || player >= s->player_count ||
        group < 0) return 0;
    for (i = 0; i < s->land_count; i++) {
        int level;
        if (s->land_type[i] != 0 || s->land_owner[i] != player ||
            ai_roadblock_land_name_group(s, projection, i) != group)
            continue;
        level = s->land_level[i];
        if (level < 0 || level >= 6) continue;
        value += s->land_toll_by_level[i][level];
    }
    return value * (s->price_index > 0 ? s->price_index : 1);
}

/* 0x42107F (tool ID 2) has two distinct phases. A non-branching four-node
 * walk checks direct land/facility/special candidates. If it branches or
 * finds no direct target, the handler intersects a six-node 0x40B343 path
 * with the visible route table and scores the AI's owned land groups. */
static int ai_roadblock_tool_command(
    const GameState *s, int p, Rich4Rng *rng,
    const GameAiGlobalProjection *projection, GameCommand *out,
    GameAiCandidateResult *reason)
{
    int direct_nodes[8];
    int direct_count;
    int branched = 0;
    int selected_node = -1;
    int unknown_direct = 0;
    int i;

    direct_count = ai_collect_original_route_window(
        s, s->players[p].node, s->players[p].previous_node,
        4, rng, direct_nodes, &branched);
    if (direct_count <= 0) {
        *reason = GAME_AI_CANDIDATE_DEFERRED_TARGET;
        return 0;
    }

    if (!branched) {
        for (i = 0; i < direct_count; i++) {
            int node = direct_nodes[i];
            uint32_t flags;
            uint16_t raw;
            int64_t funds;

            if (node <= 0 || node >= s->tool_route_node_count) continue;
            flags = s->tool_route_flags[node];
            if (ai_route_has_occupant(s, projection, node, 0x003fff00u)) continue;
            raw = s->route_object_raw[node];
            funds = (int64_t)s->players[p].cash + s->players[p].bank;

            if (raw > 2000u && raw < 4000u) {
                int id = (int)raw - 2001;
                int owned_group_count = 0;
                int group;
                int j;

                if (id < 0 || id >= s->land_count ||
                    s->land_owner[id] != -1 ||
                    funds <= 10000 ||
                    s->players[p].god_investment_modifier < 0 ||
                    s->players[p].days_tortoise_walking != 0 ||
                    (int64_t)s->price_index * s->land_price[id] >=
                        s->players[p].cash)
                    continue;
                if (s->land_level[id] == 0) {
                    group = ai_roadblock_land_name_group(
                        s, projection, id);
                    if (group < 0) {
                        /* SDL supplies original map-name groups for every
                         * land class. Core-only state proves type-0 groups. */
                        unknown_direct = 1;
                        break;
                    }
                    for (j = 0; j < s->land_count; j++) {
                        if (ai_roadblock_land_name_group(
                                s, projection, j) == group &&
                            s->land_owner[j] == p)
                            owned_group_count++;
                    }
                    if (owned_group_count < 2) continue;
                }
                selected_node = node;
                break;
            }

            if (raw > 4000u && raw < 6000u) {
                int id = (int)raw - 4001;
                if (id < 0 || id >= s->facility_count ||
                    s->facility_owner[id] != -1 ||
                    funds <= 10000 ||
                    s->players[p].god_investment_modifier < 0 ||
                    s->players[p].days_tortoise_walking != 0 ||
                    (int64_t)s->price_index * s->facility_price[id] >=
                        s->players[p].cash)
                    continue;
                selected_node = node;
                break;
            }

            /* 0x42107F's non-property branch accepts the route's low-byte
             * action 15 when the actor has more than 200 original points. */
            if ((flags & 0xffu) == 15u && s->players[p].points > 200) {
                selected_node = node;
                break;
            }
        }
    }

    if (selected_node > 0) {
        *out = rich4_game_domain_command(
            GAME_COMMAND_USE_TOOL, GAME_COMMAND_SOURCE_AI, p,
            rich4_game_domain_ref(GAME_DOMAIN_TOOL, 1),
            rich4_game_domain_ref(GAME_DOMAIN_ROUTE_NODE, selected_node), 0);
        if (rich4_tool_inspect(s, out) == GAME_TOOL_USE_APPLIED) return 1;
        *reason = GAME_AI_CANDIDATE_RULE_REJECTED;
        return 0;
    }
    if (unknown_direct) {
        *reason = GAME_AI_CANDIDATE_DEFERRED_TARGET;
        return 0;
    }
    if (projection == NULL) {
        *reason = GAME_AI_CANDIDATE_DEFERRED_TARGET;
        return 0;
    }

    {
        int path_nodes[8];
        int path_count;
        int visible_nodes[GAME_MAX_MAP_ROUTE_NODES];
        int visible_count;
        int best_node = -1;
        int64_t best_score = 0;
        int64_t score_threshold;

        if (s->price_index <= 0) {
            *reason = GAME_AI_CANDIDATE_DEFERRED_TARGET;
            return 0;
        }
        {
            int start_node = s->players[p].previous_node;
            int previous_node = s->players[p].node;
            if (start_node == 0) start_node = s->players[p].next_node;
            path_count = ai_collect_original_route_window(
                s, start_node, previous_node, 6, rng, path_nodes, NULL);
        }
        if (path_count <= 0) {
            *reason = GAME_AI_CANDIDATE_DEFERRED_TARGET;
            return 0;
        }
        visible_count = ai_collect_global_route_nodes(
            s, projection, visible_nodes, GAME_MAX_MAP_ROUTE_NODES);
        if (visible_count < 0) {
            *reason = GAME_AI_CANDIDATE_DEFERRED_TARGET;
            return 0;
        }
        score_threshold = (int64_t)6000 * s->price_index;
        for (i = 0; i < visible_count; i++) {
            int node = visible_nodes[i];
            uint16_t raw;
            int id;
            int64_t score;
            if (!ai_route_window_contains(path_nodes, path_count, node))
                continue;
            raw = s->route_object_raw[node];
            if (raw <= 2000u || raw >= 4000u) continue;
            id = (int)raw - 2001;
            if (id < 0 || id >= s->land_count ||
                s->land_owner[id] != p)
                continue;
            {
                int group = ai_roadblock_land_name_group(
                    s, projection, id);
                if (group < 0) {
                    /* 0x419744 receives the raw name pointer; missing map
                     * names cannot be reconstructed from core state. */
                    *reason = GAME_AI_CANDIDATE_DEFERRED_TARGET;
                    return 0;
                }
                score = ai_roadblock_land_group_value(s, projection, p,
                                                      group);
            }
            if (score > score_threshold && score > best_score) {
                best_score = score;
                best_node = node;
            }
        }
        if (best_node > 0) {
            *out = rich4_game_domain_command(
                GAME_COMMAND_USE_TOOL, GAME_COMMAND_SOURCE_AI, p,
                rich4_game_domain_ref(GAME_DOMAIN_TOOL, 1),
                rich4_game_domain_ref(GAME_DOMAIN_ROUTE_NODE, best_node), 0);
            if (rich4_tool_inspect(s, out) == GAME_TOOL_USE_APPLIED) return 1;
            *reason = GAME_AI_CANDIDATE_RULE_REJECTED;
            return 0;
        }
    }
    *reason = GAME_AI_CANDIDATE_RULE_REJECTED;
    return 0;
}

static int ai_route_tool_command(const GameState *s, int p, int tool,
                                 Rich4Rng *rng,
                                 const GameAiGlobalProjection *projection,
                                 GameCommand *out,
                                 GameAiCandidateResult *reason)
{
    if (tool == 1)
        return ai_roadblock_tool_command(s, p, rng, projection, out, reason);
    if (tool == 2 || tool == 3)
        return ai_route_object_tool_command(s, p, tool, rng, projection,
                                             out, reason);
    *reason = GAME_AI_CANDIDATE_DEFERRED_TARGET;
    return 0;
}

/* 0x421717 first asks 0x40D2D3 for the relation-preferred player.  When
 * there is no preferred result it uses 0x40D31C, which builds a list of
 * active, non-hotel players and chooses one with the same RNG stream.  The
 * subsequent 0x40A0B1 screen check is intentionally performed by the caller
 * for only that selected center; the original does not retry another player
 * after the 200x200 object test fails. */
/* 0x496B9A packs player-record bytes +50..+53. */
static int ai_actor_status_clear(const GamePlayerState *player)
{
    return player->delayed_status_block == 0 &&
           player->days_disappearing == 0 && player->days_in_prison == 0 &&
           player->days_in_hospital == 0;
}

static int ai_area_target_player(const GameState *s, int p, Rich4Rng *rng)
{
    int preferred = relation_preferred_player(s, p);
    int candidates[GAME_MAX_PLAYERS];
    int count = 0;
    int i;

    if (preferred >= 0)
        return preferred;
    for (i = 0; i < s->player_count; i++) {
        if (i == p || !rich4_tool_active_player(s, i) ||
            !ai_actor_status_clear(&s->players[i]))
            continue;
        if (count < GAME_MAX_PLAYERS) candidates[count++] = i;
    }
    if (count == 0) return -1;
    return candidates[rich4_rng_original_state_range(rng, count)];
}

static int ai_nuclear_property_screen(
    const GameState *s, int actor, GameDomainRef center,
    const GameAiGlobalProjection *projection, uint16_t *screen_cells);

/* fcn_00421E62 first builds a candidate list from built land and facilities
 * owned by someone other than the acting player.  The candidate list is in
 * typed-record order (all land records, then all facility records); the
 * 440x440 object table is used only by the later density screen.  Project
 * that typed candidate list here so nuclear missiles no longer have to
 * collapse to a player center whenever the property tables are available.
 * Return -1 for the small states that have no property projection yet, 0 for
 * a known empty candidate list, and 1 on success. */
static int ai_nuclear_built_property_target(const GameState *s, int p,
                                            Rich4Rng *rng,
                                            const GameAiGlobalProjection *projection,
                                            GameDomainRef *out)
{
    GameDomainRef candidates[GAME_MAX_MAP_PROPERTIES * 2];
    uint16_t *screen_cells = NULL;
    int count = 0;
    int i;
    int attempt;

    if (s == NULL || rng == NULL || out == NULL) return -1;
    if (s->land_count <= 0 && s->facility_count <= 0) return -1;
    for (i = 0; i < s->land_count && i < GAME_MAX_MAP_PROPERTIES; i++) {
        int owner = s->land_owner[i];
        if (owner >= 0 && owner < s->player_count && owner != p &&
            s->land_level[i] > 0)
            candidates[count++] =
                rich4_game_domain_ref(GAME_DOMAIN_LAND, i);
    }
    for (i = 0; i < s->facility_count && i < GAME_MAX_MAP_PROPERTIES; i++) {
        int owner = s->facility_owner[i];
        if (owner >= 0 && owner < s->player_count && owner != p &&
            s->facility_level[i] > 0)
            candidates[count++] =
                rich4_game_domain_ref(GAME_DOMAIN_FACILITY, i);
    }
    if (count == 0) return 0;
    if (projection != NULL) {
        screen_cells = (uint16_t *)malloc(440u * 440u * sizeof(*screen_cells));
        if (screen_cells == NULL) return 0;
    }
    for (attempt = 0; attempt < 10; attempt++) {
        GameDomainRef candidate =
            candidates[rich4_rng_original_state_range(rng, count)];
        int allowed = ai_nuclear_property_screen(
            s, p, candidate, projection, screen_cells);
        if (allowed == -1) {
            free(screen_cells);
            return -1;
        }
        if (allowed == 1) {
            *out = candidate;
            free(screen_cells);
            return 1;
        }
    }
    free(screen_cells);
    return 0;
}

static int ai_raw_object_owned_by_player(const GameState *s, uint16_t raw,
                                         int player)
{
    int index;
    if (raw > 0x7d0 && raw < 0xfa0) {
        index = (int)raw - 0x7d1;
        return index >= 0 && index < s->land_count &&
               s->land_owner[index] == player;
    }
    if (raw > 0xfa0 && raw < 0x1770) {
        index = (int)raw - 0xfa1;
        return index >= 0 && index < s->facility_count &&
               s->facility_owner[index] == player;
    }
    if (raw > 0x1770 && raw < 0x1f40) {
        index = (int)raw - 0x1771;
        return index >= 0 && index < s->company_count &&
               index < GAME_MAX_MAP_PROPERTIES &&
               s->company_owner[index] == player;
    }
    return 0;
}

static int ai_full_object_cell(const GameAiGlobalProjection *projection,
                               int world_x, int world_y, int *out_cell)
{
    int screen_x;
    int screen_y;
    if (projection == NULL || out_cell == NULL ||
        !ai_global_projection_screen_xy(projection, world_x, world_y,
                                        &screen_x, &screen_y))
        return 0;
    *out_cell = screen_y * 440 + screen_x;
    return 1;
}

/* Rebuild the full 440x440 object buffer used by 0x421E62 through
 * 0x40A0B1(...,-1).  That helper inserts the current actor, then all owned
 * land records, then all owned facilities; later records replace earlier
 * records at the same pixel.  The subsequent handler scans this buffer in
 * row-major order, rejects any actor, and compares the selected target's
 * built-property count/level against the remaining projected properties. */
static int ai_nuclear_property_screen_projected(
    const GameState *s, int actor,
    const GameAiGlobalProjection *projection, uint16_t *screen_cells,
    int center_x, int center_y,
    int active_players)
{
    GameAiGlobalProjection centered = *projection;
    int cell;
    int current_count = 0;
    int other_count = 0;
    int current_level = 0;
    int target_level = 0;
    int i;

    if (screen_cells == NULL) return 0;
    centered.center_world_x = center_x;
    centered.center_world_y = center_y;
    memset(screen_cells, 0, 440u * 440u * sizeof(*screen_cells));

    if (actor >= 0 && actor < s->player_count &&
        ai_actor_status_clear(&s->players[actor]) &&
        (projection->positioned_player_mask & (1u << actor)) != 0 &&
        ai_full_object_cell(&centered,
            projection->player_world_x[actor],
            projection->player_world_y[actor], &cell))
        screen_cells[cell] = (uint16_t)(0x8000u | (1u << actor));

    for (i = 0; i < s->land_count && i < GAME_MAX_MAP_PROPERTIES; i++) {
        if (s->land_owner[i] >= 0 && ai_full_object_cell(
                &centered, s->tool_land_x[i], s->tool_land_y[i], &cell))
            screen_cells[cell] = (uint16_t)(0x7d1u + i);
    }
    for (i = 0; i < s->facility_count && i < GAME_MAX_MAP_PROPERTIES; i++) {
        if (s->facility_owner[i] >= 0 && ai_full_object_cell(
                &centered, s->tool_facility_x[i], s->tool_facility_y[i],
                &cell))
            screen_cells[cell] = (uint16_t)(0xfa1u + i);
    }

    for (i = 0; i < 440 * 440; i++) {
        uint16_t raw = screen_cells[i];
        int property_id;
        int level;
        int owner;
        if (raw == 0) continue;
        if ((raw & 0x8000u) != 0) return 0;
        if (raw > 0x7d0u && raw < 0xfa0u) {
            property_id = (int)raw - 0x7d1;
            if (property_id < 0 || property_id >= s->land_count) continue;
            owner = s->land_owner[property_id];
            level = s->land_level[property_id];
        } else if (raw > 0xfa0u && raw < 0x1770u) {
            property_id = (int)raw - 0xfa1;
            if (property_id < 0 || property_id >= s->facility_count) continue;
            owner = s->facility_owner[property_id];
            level = s->facility_level[property_id];
        } else {
            continue;
        }
        if (owner == actor) {
            current_count++;
            current_level += level;
        } else {
            other_count++;
            target_level += level;
        }
    }
    return (float)((double)current_count / other_count) <
               (float)(1.0 / (active_players + 2)) &&
           (float)((double)current_level / target_level) <
               (float)(1.0 / (active_players + 2));
}

/* 0x421E62 requires the projected 0x40A0B1 object buffer. */
static int ai_nuclear_property_screen(
    const GameState *s, int actor, GameDomainRef center,
    const GameAiGlobalProjection *projection, uint16_t *screen_cells)
{
    int center_x;
    int center_y;
    int active_players = 0;
    int i;

    if (projection == NULL || !rich4_tool_target_xy(
            s, center, &center_x, &center_y)) return -1;
    for (i = 0; i < s->player_count; i++)
        if (rich4_tool_active_player(s, i)) active_players++;
    return ai_nuclear_property_screen_projected(
        s, actor, projection, screen_cells, center_x, center_y, active_players);
}

static int ai_local_object_cell(const GameAiGlobalProjection *projection,
                                int world_x, int world_y, int *out_cell)
{
    int screen_x;
    int screen_y;
    if (projection == NULL || out_cell == NULL ||
        !ai_global_projection_screen_xy(projection, world_x, world_y,
                                        &screen_x, &screen_y) ||
        screen_x < 120 || screen_x >= 320 ||
        screen_y < 120 || screen_y >= 320)
        return 0;
    *out_cell = (screen_y - 120) * 200 + (screen_x - 120);
    return 1;
}

/* 0x40A0B1 writes the current actor, then owned land records, then owned
 * facilities into a 440x440 buffer. Its n100=100 result is the 200x200
 * pixel window at rows/columns 120..319. Later property writes replace an
 * actor or property at the same pixel. 0x421717 accepts the target when the
 * window has no actor and no property owned by the acting player; an empty
 * window is valid too. */
static int ai_area_local_projection_allowed(
    const GameState *s, int actor, int target_player,
    const GameAiGlobalProjection *projection)
{
    uint16_t cells[200 * 200];
    GameAiGlobalProjection local_projection;
    int center_x;
    int center_y;
    int target_node;
    int cell;
    int i;

    if (s == NULL || projection == NULL || actor < 0 ||
        actor >= s->player_count || target_player < 0 ||
        target_player >= s->player_count)
        return -1;
    if ((projection->positioned_player_mask & (1u << target_player)) != 0) {
        center_x = projection->player_world_x[target_player];
        center_y = projection->player_world_y[target_player];
    } else {
        target_node = s->players[target_player].node;
        if (s->players[target_player].confinement_actor_type ==
            GAME_CONFINEMENT_PRISON)
            target_node = s->prison_node;
        else if (s->players[target_player].confinement_actor_type ==
                 GAME_CONFINEMENT_HOSPITAL)
            target_node = s->hospital_node;
        if (!rich4_tool_valid_node(s, target_node)) return -1;
        center_x = s->tool_route_x[target_node];
        center_y = s->tool_route_y[target_node];
    }
    local_projection = *projection;
    local_projection.center_world_x = center_x;
    local_projection.center_world_y = center_y;
    memset(cells, 0, sizeof(cells));

    /* The original local-table helper inserts only the current actor. */
    if (ai_actor_status_clear(&s->players[actor]) &&
        (projection->positioned_player_mask & (1u << actor)) != 0 &&
        ai_local_object_cell(&local_projection,
            projection->player_world_x[actor],
            projection->player_world_y[actor], &cell))
        cells[cell] = (uint16_t)(0x8000u | (1u << actor));

    for (i = 0; i < s->land_count && i < GAME_MAX_MAP_PROPERTIES; i++) {
        if (s->land_owner[i] >= 0 && ai_local_object_cell(
                &local_projection, s->tool_land_x[i], s->tool_land_y[i],
                &cell))
            cells[cell] = (uint16_t)(0x7d1u + i);
    }
    for (i = 0; i < s->facility_count && i < GAME_MAX_MAP_PROPERTIES; i++) {
        if (s->facility_owner[i] >= 0 && ai_local_object_cell(
                &local_projection, s->tool_facility_x[i],
                s->tool_facility_y[i], &cell))
            cells[cell] = (uint16_t)(0xfa1u + i);
    }
    for (i = 0; i < (int)(sizeof(cells) / sizeof(cells[0])); i++) {
        uint16_t raw = cells[i];
        if ((raw & 0x8000u) != 0 ||
            ai_raw_object_owned_by_player(s, raw, actor))
            return 0;
    }
    return 1;
}

/* The core-only fallback has no active view projection; retain a typed
 * coordinate approximation for those synthetic states. */
static int ai_area_target_local_screen_allowed(
    const GameState *s, int actor, int target_player,
    const GameAiGlobalProjection *projection)
{
    AiProjectedObject objects[AI_PROJECTED_OBJECT_CAPACITY];
    int object_count;
    int target_node;
    int center_x;
    int center_y;
    int i;

    if (s == NULL || target_player < 0 || target_player >= s->player_count ||
        s->tool_route_node_count <= 1)
        return -1;
    if (projection != NULL)
        return ai_area_local_projection_allowed(
            s, actor, target_player, projection);
    target_node = s->players[target_player].node;
    if (!rich4_tool_valid_node(s, target_node)) return -1;
    center_x = s->tool_route_x[target_node];
    center_y = s->tool_route_y[target_node];
    object_count = ai_collect_projected_objects(
        s, objects, AI_PROJECTED_OBJECT_CAPACITY);
    for (i = 0; i < object_count; i++) {
        const AiProjectedObject *object = &objects[i];
        if (!rich4_tool_in_square(object->x, object->y, center_x, center_y,
                                  100))
            continue;
        /* 0x421717 rejects an actor record or an actor-owned property, but
         * it accepts the window when every projected cell is otherwise
         * empty or occupied only by another owner. */
        if ((object->raw & 0x8000u) != 0) return 0;
        if (object->raw != 0 &&
            ai_raw_object_owned_by_player(s, object->raw, actor))
            return 0;
    }
    /* The original scan does not stop at the first harmless object.  A
     * typed property that was added by the map loader can therefore not hide
     * a later actor-owned property in the same window. */
    if (ai_property_geometry_present(s)) {
        for (i = 0; i < s->land_count && i < GAME_MAX_MAP_PROPERTIES; i++) {
            if (s->land_owner[i] == actor &&
                rich4_tool_in_square(s->tool_land_x[i], s->tool_land_y[i],
                                     center_x, center_y, 100))
                return 0;
        }
        for (i = 0; i < s->facility_count &&
                    i < GAME_MAX_MAP_PROPERTIES; i++) {
            if (s->facility_owner[i] == actor &&
                rich4_tool_in_square(s->tool_facility_x[i],
                                     s->tool_facility_y[i], center_x,
                                     center_y, 100))
                return 0;
        }
    }
    return 1;
}

/* fcn_00421717 first scans the global 0x48B8C4 table for the selected
 * ordinary-player bit.  A missing mask rejects the target; when the mask is
 * present, the original still applies the 200x200 local screen before
 * accepting it. With an SDL projection, no mask is a known rejection;
 * return -1 only for core-only synthetic states that need the typed fallback. */
static int ai_area_global_target_mask(
    const GameState *s, int target_player,
    const GameAiGlobalProjection *projection)
{
    AiProjectedObject objects[AI_PROJECTED_OBJECT_CAPACITY];
    int object_count;
    int saw_mask = 0;
    int i;

    if (s == NULL || target_player < 0 || target_player >= 4)
        return -1;
    if (projection != NULL) {
        object_count = ai_prepare_global_player_masks(
            s, objects, AI_PROJECTED_OBJECT_CAPACITY, projection);
        if (object_count < 0) return -1;
    } else {
        object_count = ai_collect_projected_objects(
            s, objects, AI_PROJECTED_OBJECT_CAPACITY);
        object_count = ai_prepare_global_objects(
            s, objects, object_count, AI_PROJECTED_OBJECT_CAPACITY, NULL);
    }
    for (i = 0; i < object_count; i++) {
        uint16_t raw = objects[i].raw;
        if ((raw & 0x8000u) == 0 || (raw & 0x000fu) == 0) continue;
        saw_mask = 1;
        if ((raw & (uint16_t)(1u << target_player)) != 0)
            return 1;
    }
    if (saw_mask || projection != NULL) return 0;
    /* In a synthetic/core state, typed records can establish that the
     * selected target's local 200x200 window contains an object, but they
     * cannot establish its global player-mask bit.  Defer that case only
     * when there is something in the local window.  An empty local scan is a
     * known rejection, as is an entirely empty 0x40A45C(-1) result. */
    if (object_count > 0 &&
        rich4_tool_valid_node(s, s->players[target_player].node)) {
        int target_node = s->players[target_player].node;
        int center_x = s->tool_route_x[target_node];
        int center_y = s->tool_route_y[target_node];
        for (i = 0; i < object_count; i++)
            if ((objects[i].raw != 0 ||
                 objects[i].ref.kind == GAME_DOMAIN_MAP_OBJECT) &&
                rich4_tool_in_square(objects[i].x, objects[i].y,
                                     center_x, center_y, 100))
                return -1;
    }
    return 0;
}

/* The original missile handlers store their selected result in the scalar
 * output at 0x48BE64 after scanning a viewport-wide object table. Area
 * missiles use the relation-first player center plus the projected object
 * screen; nuclear missiles also use the built-property candidate list above. */
static int ai_area_tool_command(const GameState *s, int p, int tool,
                                Rich4Rng *rng,
                                const GameAiGlobalProjection *projection,
                                GameCommand *out,
                                GameAiCandidateResult *reason)
{
    int target;
    int global_target_mask = -1;
    int nuclear_target = -1;
    GameDomainRef target_ref = none_ref();

    if (tool == 12) {
        nuclear_target = ai_nuclear_built_property_target(
            s, p, rng, projection, &target_ref);
        if (nuclear_target <= 0) {
            *reason = nuclear_target < 0 ? GAME_AI_CANDIDATE_DEFERRED_TARGET :
                GAME_AI_CANDIDATE_RULE_REJECTED;
            return 0;
        }
    }
    if (nuclear_target != 1) {
        target = ai_area_target_player(s, p, rng);
        if (target < 0) {
            *reason = GAME_AI_CANDIDATE_RULE_REJECTED;
            return 0;
        }
        target_ref = rich4_game_domain_ref(GAME_DOMAIN_PLAYER, target);
    }
    if (tool == 6) {
        global_target_mask = ai_area_global_target_mask(
            s, target, projection);
        if (global_target_mask == 0) {
            *reason = GAME_AI_CANDIDATE_RULE_REJECTED;
            return 0;
        }
        /* 0x421717 calls the local 0x40A0B1/0x4216AB screen after a
         * successful global-mask match as well.  Keep the same check for the
         * typed fallback when the global player mask is unavailable. */
        if (ai_area_target_local_screen_allowed(
                s, p, target, projection) == 0) {
            *reason = GAME_AI_CANDIDATE_RULE_REJECTED;
            return 0;
        }
    }
    *out = rich4_game_domain_command(
        GAME_COMMAND_USE_TOOL, GAME_COMMAND_SOURCE_AI, p,
        rich4_game_domain_ref(GAME_DOMAIN_TOOL, tool),
        target_ref, 0);
    if (rich4_tool_inspect(s, out) != GAME_TOOL_USE_APPLIED) {
        *reason = GAME_AI_CANDIDATE_RULE_REJECTED;
        return 0;
    }
    return 1;
}

static int ai_collect_linear_route_window(const GameState *s, int p,
                                          int limit, Rich4Rng *rng,
                                          int out_nodes[8]);

/* The straight-path branch of fcn_00420970 (card 30 / 乌龟卡) counts only
 * property records in the first three projected route objects.  An empty
 * land contributes its base price; an owned land contributes its next-level
 * price only when its original type byte is zero and it is below level 5.
 * Facilities use the corresponding construction charge and reject the
 * original type values that are closed to this handler.  A valuable foreign group or special
 * object sends the original routine to its viewport-wide object table.
 * Return -1 when one of those records explicitly selects that global
 * fallback; return 0 for an ordinary direct-path rejection. */
static int ai_machine_doll_direct_gate(const GameState *s, int p,
                                       const int nodes[], int count)
{
    int64_t value = 0;
    int counted = 0;
    int i;

    if (s == NULL || nodes == NULL || p < 0 || p >= s->player_count)
        return 0;
    for (i = 0; i < count; i++) {
        uint16_t raw = s->route_object_raw[nodes[i]];
        int index;
        int owner;
        if (raw > 0x7d0 && raw < 0xfa0) {
            index = (int)raw - 0x7d1;
            if (index < 0 || index >= s->land_count) continue;
            owner = s->land_owner[index];
            if (owner < 0) {
                value += s->land_price[index];
                counted++;
            } else if (owner == p) {
                if (s->land_type[index] == 0 &&
                    s->land_level[index] < 5) {
                    value += s->land_price_per_level[index];
                    counted++;
                }
            } else if (s->land_group[index] >= 0) {
                int64_t group_value = 0;
                int price_index = s->price_index > 0 ? s->price_index : 1;
                int j;
                for (j = 0; j < s->land_count; j++) {
                    int level;
                    if (s->land_group[j] != s->land_group[index] ||
                        s->land_owner[j] != owner) continue;
                    level = s->land_level[j];
                    if (level < 0 || level >= 6) continue;
                    group_value += s->land_toll_by_level[j][level];
                }
                /* fcn_00419744 returns the indexed group value; the direct
                 * 0x420970 branch falls back only above 1000 * price_index. */
                if (group_value * price_index > 1000LL * price_index)
                    return -1;
            }
            continue;
        }
        if (raw > 0xfa0 && raw < 0x1770) {
            index = (int)raw - 0xfa1;
            if (index < 0 || index >= s->facility_count) continue;
            owner = s->facility_owner[index];
            if (owner < 0) {
                value += s->facility_price[index];
                counted++;
            } else if (owner == p && s->facility_type[index] != 0 &&
                       s->facility_type[index] != 3 &&
                       s->facility_level[index] < 5) {
                /* The direct card-30 branch reads facility +0x24, i.e. the
                 * level-0 charge, even for an already-built facility. */
                value += s->facility_charge_by_level[index][0];
                counted++;
            } else if (owner != p && owner >= 0 &&
                       s->facility_type[index] != 0 &&
                       s->facility_type[index] != 4 &&
                       s->facility_level[index] > 0) {
                return -1;
            }
            continue;
        }
        /* 0x420970 ignores an unowned or self-owned auxiliary record in the
         * direct three-object path; only another player's company sends the
         * handler to its global-object fallback. */
        if (raw > 0x1770 && raw < 0x1f40) {
            index = (int)raw - 0x1771;
            if (index < 0 || index >= s->company_count ||
                index >= GAME_MAX_MAP_PROPERTIES)
                return -1;
            if (s->company_owner[index] >= 0 &&
                s->company_owner[index] != p)
                return -1;
        }
    }

    if (counted < 2 || value * 3 >= (int64_t)s->players[p].cash * 2)
        return 0;
    if ((int64_t)s->players[p].cash + s->players[p].bank <= 10000 ||
        s->players[p].god_investment_modifier < 0)
        return 0;
    return 1;
}

/* fcn_0040B221 reuses the previous route record when the path ends and
 * consumes the RNG only when a step has multiple usable edges.  Card 30 uses its
 * branch marker to enter the global table, so retain both the selected window
 * and that marker instead of silently choosing a straight path. */
static int ai_collect_card30_window(const GameState *s, int p, int limit,
                                    Rich4Rng *rng, int out_nodes[8],
                                    int *out_branched)
{
    int count;

    if (out_branched != NULL) *out_branched = 0;
    if (s == NULL || p < 0 || p >= s->player_count) return -1;
    count = ai_collect_original_route_window(
        s, s->players[p].node, s->players[p].previous_node, limit,
        rng, out_nodes, out_branched);
    return count > 0 ? count : -1;
}

/* The global half of fcn_00420970 scans each candidate's own three-node
 * route, but scores only objects owned by the acting player.  A candidate's
 * own object, or an unowned object, terminates that candidate.  This helper
 * is intentionally typed-only; an unsupported auxiliary record defers the
 * whole card rather than guessing its value. */
static int ai_card30_candidate_gate(const GameState *s, int actor,
                                    int candidate, const int nodes[], int count)
{
    int64_t value = 0;
    int counted = 0;
    int i;

    if (s == NULL || nodes == NULL || actor < 0 ||
        actor >= s->player_count || candidate < 0 ||
        candidate >= s->player_count)
        return -1;
    for (i = 0; i < count; i++) {
        uint16_t raw = s->route_object_raw[nodes[i]];
        int id;
        int owner;

        if (raw > 0x7d0u && raw < 0xfa0u) {
            id = (int)raw - 0x7d1;
            if (id < 0 || id >= s->land_count) return -1;
            owner = s->land_owner[id];
            if (owner < 0) return 0;
            if (owner == actor) {
                value += ai_machine_doll_land_group_value(s, id);
                counted++;
            } else if (owner == candidate) {
                return 0;
            }
            continue;
        }
        if (raw > 0xfa0u && raw < 0x1770u) {
            id = (int)raw - 0xfa1;
            if (id < 0 || id >= s->facility_count) return -1;
            owner = s->facility_owner[id];
            if (owner < 0) return 0;
            if (owner == actor) {
                int level = s->facility_level[id];
                if (s->facility_type[id] != 0 &&
                    s->facility_type[id] != 4 && level > 0 && level < 6) {
                    value += s->facility_charge_by_level[id][level];
                    counted++;
                }
            } else if (owner == candidate) {
                return 0;
            }
            continue;
        }
        if (raw > 0x1770u && raw < 0x1f40u) {
            id = (int)raw - 0x1771;
            if (id < 0 || id >= s->company_count ||
                id >= GAME_MAX_MAP_PROPERTIES)
                return -1;
            owner = s->company_owner[id];
            if (owner < 0) return 0;
            if (owner == actor) {
                if (s->company_base_fee[id] < 0) return -1;
                value += s->company_base_fee[id];
                counted++;
            } else if (owner == candidate) {
                return 0;
            }
        }
    }
    if (counted < 2 || value < (int64_t)(s->price_index > 0 ?
                                        s->price_index : 1) * 10000)
        return 0;
    return 1;
}

/* The fallback of 0x420970 rebuilds 0x48B8C4, collects ordinary-player masks
 * in table order, and tries each active candidate in that order.  It does not
 * choose a relation-preferred player or a strict-max property owner.  The
 * route-backed candidate list is retained only for synthetic states where
 * the renderer's high-bit actor objects are not present. */
static int ai_card30_global_gate(
    const GameState *s, int p, Rich4Rng *rng,
    const GameAiGlobalProjection *projection, int *out_target)
{
    AiProjectedObject objects[AI_PROJECTED_OBJECT_CAPACITY];
    int candidates[GAME_MAX_PLAYERS];
    unsigned char seen[GAME_MAX_PLAYERS];
    int object_count;
    int candidate_count = 0;
    int saw_player_mask = 0;
    int i;
    int bit;

    if (out_target != NULL) *out_target = -1;
    if (s == NULL || rng == NULL || p < 0 || p >= s->player_count)
        return -1;
    memset(seen, 0, sizeof(seen));
    if (projection != NULL) {
        object_count = ai_prepare_global_player_masks(
            s, objects, AI_PROJECTED_OBJECT_CAPACITY, projection);
        if (object_count < 0) return -1;
    } else {
        object_count = ai_collect_projected_objects(
            s, objects, AI_PROJECTED_OBJECT_CAPACITY);
        object_count = ai_prepare_global_objects(
            s, objects, object_count, AI_PROJECTED_OBJECT_CAPACITY, NULL);
    }
    for (i = 0; i < object_count; i++) {
        uint16_t raw = objects[i].raw;
        if ((raw & 0x8000u) == 0 || (raw & 0x000fu) == 0) continue;
        saw_player_mask = 1;
        for (bit = 0; bit < s->player_count && bit < GAME_MAX_PLAYERS &&
                    bit < 4; bit++) {
            if ((raw & (uint16_t)(1u << bit)) == 0 || bit == p ||
                seen[bit] || !rich4_tool_active_player(s, bit))
                continue;
            seen[bit] = 1;
            candidates[candidate_count++] = bit;
        }
    }
    if (!saw_player_mask && projection == NULL) {
        for (i = 0; i < s->player_count && i < GAME_MAX_PLAYERS; i++) {
            if (i == p || seen[i] || !rich4_tool_active_player(s, i) ||
                !rich4_tool_valid_node(s, s->players[i].node))
                continue;
            seen[i] = 1;
            candidates[candidate_count++] = i;
        }
    }
    if (candidate_count == 0) {
        if (projection != NULL) return 0;
        /* A typed property table alone is not enough: the original branch
         * still needs an ordinary-player object mask and a candidate route. */
        if (!saw_player_mask && !ai_property_geometry_present(s) &&
            !s->tool_company_geometry_available)
            return -1;
        return saw_player_mask ? 0 : -1;
    }

    for (i = 0; i < candidate_count; i++) {
        int nodes[8];
        int branched = 0;
        int count;
        int gate;

        count = ai_collect_card30_window(
            s, candidates[i], 3, rng, nodes, &branched);
        if (count < 0) return -1;
        if (branched) continue;
        gate = ai_card30_candidate_gate(
            s, p, candidates[i], nodes, count);
        if (gate > 0) {
            if (out_target != NULL) *out_target = candidates[i];
            return 1;
        }
        if (gate < 0) return -1;
    }
    return 0;
}

/* Card 30 shares the original 0x420970 control flow: an eligible straight
 * three-object path targets the actor; a branch or failed direct gate enters
 * the global ordinary-player candidate scan.  If that scan cannot be rebuilt,
 * preserve the deferred result instead of inventing a self-target fallback. */
static int ai_card30_direct_gate(const GameState *s, int p, Rich4Rng *rng,
                                 const GameAiGlobalProjection *projection,
                                 int *out_target)
{
    int nodes[8];
    int count;
    int branched = 0;
    int direct;
    int global;
    int global_target;

    if (out_target != NULL) *out_target = -1;
    if (s == NULL || p < 0 || p >= s->player_count)
        return -1;
    count = ai_collect_card30_window(s, p, 3, rng, nodes, &branched);
    if (count <= 0) return -1;
    if (branched) {
        global = ai_card30_global_gate(s, p, rng, projection,
                                       &global_target);
        if (global > 0 && out_target != NULL) *out_target = global_target;
        return global;
    }
    direct = ai_machine_doll_direct_gate(s, p, nodes, count);
    if (direct > 0) {
        if (out_target != NULL) *out_target = p;
        return 1;
    }
    /* Both an explicit global-fallback condition and an ordinary direct-path
     * rejection converge on LABEL_17 in the original routine. */
    global = ai_card30_global_gate(s, p, rng, projection, &global_target);
    if (global > 0 && out_target != NULL) *out_target = global_target;
    return global;
}

static int ai_no_target_tool_eligibility(const GameState *s, int p, int tool,
                                         Rich4Rng *rng,
                                         const GameAiGlobalProjection *projection,
                                         GameAiCandidateResult *reason)
{
    int traffic = s->players[p].traffic_method & 3;

    /* 0x420EFA is the one-based machine-doll tool-table slot.  Its route
     * eligibility is independent from card 30's 0x420970 handler. */
    if (tool == 0) {
        int gate = ai_machine_doll_route_gate(s, p, rng, projection);
        if (gate < 0) {
            *reason = GAME_AI_CANDIDATE_DEFERRED_TARGET;
            return 0;
        }
        if (gate == 0) {
            *reason = GAME_AI_CANDIDATE_RULE_REJECTED;
            return 0;
        }
        return 1;
    }

    /* 0x421644/0x421675/0x421E20 are AI-only gates for the three
     * no-target vehicle tools.  The public inspector still owns the final
     * inventory and command validation. */
    if (tool == 4) {
        if (traffic != 0) {
            *reason = GAME_AI_CANDIDATE_RULE_REJECTED;
            return 0;
        }
        if (rich4_rng_original_state_range(rng, 4) != 0) {
            *reason = GAME_AI_CANDIDATE_RANDOM_GATE;
            return 0;
        }
    } else if (tool == 5) {
        if (traffic >= 2) {
            *reason = GAME_AI_CANDIDATE_RULE_REJECTED;
            return 0;
        }
        if (rich4_rng_original_state_range(rng, 4) != 0) {
            *reason = GAME_AI_CANDIDATE_RANDOM_GATE;
            return 0;
        }
    } else if (tool == 11) {
        if (traffic == 3) {
            *reason = GAME_AI_CANDIDATE_RULE_REJECTED;
            return 0;
        }
        if (rich4_rng_original_state_range(rng, 15) > s->players[p].ai_personality) {
            *reason = GAME_AI_CANDIDATE_RANDOM_GATE;
            return 0;
        }
    } else if (tool == 7) {
        /* 0x421827 performs these checks before it enters the six-candidate
         * target scan.  The cheap player-record gates are exact: god slots
         * 7/8/15, the +57 tortoise-walking byte, insufficient liquidity, and
         * a negative +70 investment modifier all reject. */
        if (s->players[p].god_info == 7 ||
            s->players[p].god_info == 8 ||
            s->players[p].god_info == 15 ||
            s->players[p].days_tortoise_walking != 0) {
            *reason = GAME_AI_CANDIDATE_RULE_REJECTED;
            return 0;
        }
        if ((int64_t)s->players[p].cash + s->players[p].bank < 10000) {
            *reason = GAME_AI_CANDIDATE_NO_BUDGET;
            return 0;
        }
        if (s->players[p].god_investment_modifier < 0) {
            *reason = GAME_AI_CANDIDATE_RULE_REJECTED;
            return 0;
        }
    }
    return 1;
}

/* fcn_0040B221 returns a nonzero branch marker when any step in the forward
 * window has more than one usable edge.  0x421827 treats that marker as a
 * failed remote-dice candidate instead of choosing one edge at random.  The
 * general route helper above intentionally randomizes for the other AI tools,
 * so keep this exact, deterministic variant local to the original handler. */
static int ai_collect_linear_route_window(const GameState *s, int p,
                                          int limit, Rich4Rng *rng,
                                          int out_nodes[8])
{
    int branched = 0;
    int count;

    if (s == NULL || out_nodes == NULL || p < 0 || p >= s->player_count)
        return -1;
    count = ai_collect_original_route_window(
        s, s->players[p].node, s->players[p].previous_node, limit,
        rng, out_nodes, &branched);
    if (count <= 0) return -1;
    return branched ? -2 : count;
}

/* Project the land and facility raw-ID cases in 0x421827's six-node scan.
 * Other raw-ID ranges never become candidates in the original handler.
 * Ordinary land compares fixed-width names and writes the selected one-based
 * step to 0x48BE64. An owned facility tests its fixed +0x24 charge field
 * (charge_by_level[0]), even when its current level is higher. Return -1 for
 * an absent projection, -2 for an incomplete property projection, 0 for a
 * known empty set, and 1 with the selected step. */
static int ai_remote_dice_price_fits_cash(int64_t price, int64_t cash)
{
    if (price < 0 || cash <= 0) return 0;
    /* Equivalent to price * 5 < cash * 2 without overflowing either side. */
    return price <= (cash * 2 - 1) / 5;
}

static int ai_remote_dice_owned_name_count(
    const GameState *s, const GameAiGlobalProjection *projection,
    int player, int group)
{
    int count = 0;
    int unknown = 0;
    int i;

    if (group < 0) return -1;
    for (i = 0; i < s->land_count; i++) {
        int other_group;
        if (s->land_owner[i] != player) continue;
        other_group = ai_roadblock_land_name_group(s, projection, i);
        if (other_group < 0) {
            unknown = 1;
            continue;
        }
        if (other_group == group) count++;
    }
    /* Unknown owned names can affect only the >1 threshold while the
     * confirmed set contains at most one matching record. */
    return count > 1 || !unknown ? count : -1;
}

static int ai_remote_dice_projected_candidate(
    const GameState *s, const GameAiGlobalProjection *projection, int p,
    Rich4Rng *rng, int *out_value)
{
    int nodes[8];
    int count;
    int i;
    int best_level = 0;
    int best_step = 0;
    int incomplete = 0;
    int64_t cash;

    if (out_value != NULL) *out_value = 0;
    if (s == NULL || p < 0 || p >= s->player_count ||
        s->tool_route_node_count <= 1)
        return -1;
    count = ai_collect_linear_route_window(s, p, 6, rng, nodes);
    if (count == -1) return -1;
    if (count == -2) return 0;
    if (count == 0) return 0;
    cash = s->players[p].cash;
    for (i = 0; i < count; i++) {
        uint16_t raw = s->route_object_raw[nodes[i]];
        int index;
        int same_group_owned;

        /* 0x421827 first rejects nonzero bits 12..15.  A nonzero object
         * selector in bits 16..21 then goes through ref_00496d08; only the
         * original special-object categories are skipped.  Ordinary typed
         * property records are still evaluated when their selector is a
         * non-special category. */
        if ((s->tool_route_flags[nodes[i]] & 0x0000f000u) != 0)
            continue;
        if ((s->tool_route_flags[nodes[i]] & 0x003f0000u) != 0) {
            int object_type = ai_original_route_object_type(s, nodes[i]);
            if (object_type < 0) {
                /* With an unknown category the original may skip a land or
                 * facility before pricing it, so that case stays deferred.
                 * Non-property raw IDs cannot become a candidate either way. */
                if ((raw > 0x7d0u && raw < 0xfa0u) ||
                    (raw > 0xfa0u && raw < 0x1770u))
                    return -2;
                continue;
            }
            if (object_type == 5 || object_type == 6 ||
                object_type == 7 || object_type == 8 ||
                object_type == 10 || object_type == 11 ||
                object_type == 16 || object_type == 17 ||
                object_type == 18)
                continue;
        }
        if (raw > 0x7d0 && raw < 0xfa0) {
            int owner;
            int level;
            int group;
            int64_t price;
            index = (int)raw - 0x7d1;
            if (index < 0 || index >= s->land_count) {
                incomplete = 1;
                continue;
            }
            owner = s->land_owner[index];
            level = s->land_level[index];
            /* 0x421827 accepts unowned land immediately when the actor
             * owns at least two other records with the same name. */
            if (owner < 0) {
                group = ai_roadblock_land_name_group(s, projection, index);
                if (group < 0) { incomplete = 1; continue; }
                same_group_owned = ai_remote_dice_owned_name_count(
                    s, projection, p, group);
                if (same_group_owned < 0) { incomplete = 1; continue; }
                if (same_group_owned <= 1 || !ai_remote_dice_price_fits_cash(
                        s->land_price[index], cash)) continue;
                if (incomplete) return -2;
                if (out_value != NULL) *out_value = i + 1;
                return 1;
            }
            if (owner == p) {
                price = s->land_price_per_level[index];
                if (s->land_type[index] != 0 || level >= 5 ||
                    level <= best_level ||
                    !ai_remote_dice_price_fits_cash(price, cash))
                    continue;
                group = ai_roadblock_land_name_group(s, projection, index);
                if (group < 0) {
                    incomplete = 1;
                    continue;
                }
                same_group_owned = ai_remote_dice_owned_name_count(
                    s, projection, p, group);
                if (same_group_owned < 0) {
                    incomplete = 1;
                    continue;
                }
                if (same_group_owned <= 1) continue;
                best_level = level;
                best_step = i + 1;
            } else {
                continue;
            }
        }
        if (raw > 0xfa0 && raw < 0x1770) {
            int owner;
            int level;
            int64_t price;
            index = (int)raw - 0xfa1;
            if (index < 0 || index >= s->facility_count) {
                incomplete = 1;
                continue;
            }
            owner = s->facility_owner[index];
            level = s->facility_level[index];
            if (owner < 0) {
                price = s->facility_price[index];
                if (!ai_remote_dice_price_fits_cash(price, cash))
                    continue;
                if (incomplete) return -2;
                if (out_value != NULL) *out_value = i + 1;
                return 1;
            } else if (owner == p) {
                if (s->facility_type[index] == 0 ||
                    s->facility_type[index] == 3 || level < 0 || level >= 5)
                    continue;
                /* 0x421827 reads fixed facility +0x24, charge level zero. */
                price = s->facility_charge_by_level[index][0];
                if (!ai_remote_dice_price_fits_cash(price, cash))
                    continue;
                if (incomplete) return -2;
                if (out_value != NULL) *out_value = i + 1;
                return 1;
            } else {
                continue;
            }
        }
    }
    if (incomplete) return -2;
    if (best_step != 0) {
        if (out_value != NULL) *out_value = best_step;
        return 1;
    }
    return 0;
}

/* 0x421CB6 searches the full visible road table and retains the first
 * property at the highest qualifying level. */
static int ai_transfer_tool_command(
    const GameState *s, int p, Rich4Rng *rng,
    const GameAiGlobalProjection *projection, GameCommand *out,
    GameAiCandidateResult *reason)
{
    int property_gate;
    int property_node = -1;

    (void)rng;

    if ((int64_t)s->players[p].cash + s->players[p].bank <= 10000) {
        *reason = GAME_AI_CANDIDATE_NO_BUDGET;
        return 0;
    }
    if (s->players[p].god_investment_modifier < 0) {
        *reason = GAME_AI_CANDIDATE_RULE_REJECTED;
        return 0;
    }
    if (!rich4_tool_active_player(s, p) ||
        !rich4_tool_valid_node(s, s->players[p].node)) {
        *reason = GAME_AI_CANDIDATE_RULE_REJECTED;
        return 0;
    }
    property_gate = ai_transfer_visible_property_gate(
        s, p, projection, &property_node);
    if (property_gate == 0) {
        *reason = GAME_AI_CANDIDATE_RULE_REJECTED;
        return 0;
    }
    if (property_gate < 0) {
        *reason = GAME_AI_CANDIDATE_DEFERRED_TARGET;
        return 0;
    }
    if (property_gate == 1 && property_node >= 0) {
        *out = rich4_game_domain_command(
            GAME_COMMAND_USE_TOOL, GAME_COMMAND_SOURCE_AI, p,
            rich4_game_domain_ref(GAME_DOMAIN_TOOL, 10),
            rich4_game_domain_ref(GAME_DOMAIN_PLAYER, p), 0);
        out->target2 = rich4_game_domain_ref(
            GAME_DOMAIN_ROUTE_NODE, property_node);
        if (rich4_tool_inspect(s, out) == GAME_TOOL_USE_APPLIED)
            return 1;
    }
    *reason = GAME_AI_CANDIDATE_RULE_REJECTED;
    return 0;
}

static int tool_command(const GameState *s, int p, int tool, Rich4Rng *rng,
                        const GameAiGlobalProjection *projection,
                        GameCommand *out, GameAiCandidateResult *reason)
{
    const GameToolInfo *info = rich4_tool_info(tool);
    int value = 0;
    if (info == NULL) { *reason = GAME_AI_CANDIDATE_RULE_REJECTED; return 0; }
    if (info->target_kind == GAME_TOOL_TARGET_ROUTE_NODE)
        return ai_route_tool_command(s, p, tool, rng, projection, out,
                                     reason);
    if (tool == 8)
        return ai_robot_command(s, p, rng, projection, out, reason);
    if (info->target_kind == GAME_TOOL_TARGET_AREA)
        return ai_area_tool_command(s, p, tool, rng, projection, out,
                                    reason);
    if (info->target_kind == GAME_TOOL_TARGET_TRANSFER)
        return ai_transfer_tool_command(s, p, rng, projection, out, reason);
    if (!ai_no_target_tool_eligibility(s, p, tool, rng, projection, reason))
        return 0;
    if (info->target_kind != GAME_TOOL_TARGET_NONE) {
        *reason = GAME_AI_CANDIDATE_DEFERRED_TARGET;
        return 0;
    }
    /* Original ID 8 (zero-based tool 7) writes the selected candidate's
     * one-based position in the six-node scan to 0x48BE64, which
     * 0x4470F8/0x420EEE later reads as the forced die value. */
    if (tool == 7) {
        int projected = ai_remote_dice_projected_candidate(
            s, projection, p, rng, &value);
        if (projected == 0) {
            *reason = GAME_AI_CANDIDATE_RULE_REJECTED;
            return 0;
        }
        if (projected < 0) {
            *reason = GAME_AI_CANDIDATE_DEFERRED_TARGET;
            return 0;
        }
    }
    *out = rich4_game_domain_command(GAME_COMMAND_USE_TOOL,
        GAME_COMMAND_SOURCE_AI, p, rich4_game_domain_ref(GAME_DOMAIN_TOOL, tool),
        none_ref(), value);
    if (rich4_tool_inspect(s, out) != GAME_TOOL_USE_APPLIED) {
        *reason = GAME_AI_CANDIDATE_RULE_REJECTED;
        return 0;
    }
    return 1;
}

static void select_tool(const GameState *s, int p, Rich4Rng *rng,
                        const GameAiGlobalProjection *projection,
                        GameAiDecision *d)
{
    int tools[GAME_TOOL_COUNT], count = 0, start, checked, i;
    if ((s->players[p].ai_action_flags & 2) == 0) {
        trace(d, none_ref(), none_ref(), GAME_AI_CANDIDATE_DISABLED, 0, 0);
        return;
    }
    for (i = 0; i < GAME_TOOL_COUNT; i++)
        if (i != 9 && s->players[p].tool_inventory[i] != 0) tools[count++] = i;
    if (!count) {
        trace(d, none_ref(), none_ref(), GAME_AI_CANDIDATE_NO_INVENTORY, 0, 0);
        return;
    }
    start = count > 4 ? rich4_rng_original_state_range(rng, count) : 0;
    for (checked = 0; checked < 4; checked++) {
        int tool = tools[(start + checked) % count];
        const GameToolInfo *info = rich4_tool_info(tool);
        GameAiCandidateResult reason = GAME_AI_CANDIDATE_RULE_REJECTED;
        GameCommand command;
        memset(&command, 0, sizeof(command));
        GameDomainRef ref = rich4_game_domain_ref(GAME_DOMAIN_TOOL, tool);
        if (info == NULL || !personality_gate(info->ai_difficulty,
                s->players[p].ai_personality, rng, &reason)) {
            trace(d, ref, none_ref(), reason, 0, 0);
            continue;
        }
        if (!tool_command(s, p, tool, rng, projection, &command, &reason)) {
            trace(d, ref, none_ref(), reason, 0, 0);
            /* 0x447D97 returns after the first eligible handler call. */
            if (command.type == GAME_COMMAND_USE_TOOL) return;
            continue;
        }
        d->command = command;
        d->has_command = 1;
        trace(d, ref, command.target, GAME_AI_CANDIDATE_SELECTED, 0, 0);
        return;
    }
}

static void select_upgrade(const GameState *s, int p, GameAiDecision *d)
{
    static const unsigned char facility_max_level[5] = {1, 5, 5, 1, 5};
    int best_value = 0, best_kind = 0, best_id = -1, i;
    for (i = 0; i < s->land_count; i++) {
        GameDomainRef ref = rich4_game_domain_ref(GAME_DOMAIN_LAND, i);
        int level = s->land_level[i];
        int value = level >= 0 && level < 6 ? s->land_toll_by_level[i][level] : 0;
        if (s->land_owner[i] != p) {
            trace(d, ref, none_ref(), GAME_AI_CANDIDATE_NOT_OWNED, 0, 0);
        } else if (s->land_type[i] != 0 || level >= 5) {
            trace(d, ref, none_ref(), GAME_AI_CANDIDATE_NOT_UPGRADABLE, value, 0);
        } else {
            trace(d, ref, none_ref(), GAME_AI_CANDIDATE_ELIGIBLE, value, 0);
            if (value > best_value) { best_value = value; best_kind = 1; best_id = i; }
        }
    }
    for (i = 0; i < s->facility_count; i++) {
        GameDomainRef ref = rich4_game_domain_ref(GAME_DOMAIN_FACILITY, i);
        int level = s->facility_level[i];
        int type = s->facility_type[i];
        int value = s->facility_price[i];
        if (s->facility_owner[i] != p) {
            trace(d, ref, none_ref(), GAME_AI_CANDIDATE_NOT_OWNED, 0, 0);
        } else if (type < 0 || type >= 5 || level < 0 ||
                   level >= facility_max_level[type]) {
            trace(d, ref, none_ref(), GAME_AI_CANDIDATE_NOT_UPGRADABLE, value, 0);
        } else {
            trace(d, ref, none_ref(), GAME_AI_CANDIDATE_ELIGIBLE, value, 0);
            if (value > best_value) { best_value = value; best_kind = 2; best_id = i; }
        }
    }
    if (best_id >= 0) {
        d->command = rich4_game_domain_command(GAME_COMMAND_UPGRADE_PROPERTY,
            GAME_COMMAND_SOURCE_AI, p,
            rich4_game_domain_ref(best_kind == 1 ? GAME_DOMAIN_LAND :
                                  GAME_DOMAIN_FACILITY, best_id), none_ref(), 0);
        d->has_command = 1;
        trace(d, d->command.subject, none_ref(), GAME_AI_CANDIDATE_SELECTED,
              best_value, 0);
    }
}

int rich4_ai_select_dice_count(const GameState *s, int p,
                              uint32_t rng_before, int *out_count,
                              uint32_t *out_rng_after)
{
    const GamePlayerState *player;
    Rich4Rng rng;
    int nodes[8];
    int traffic, count, useful = 0, opponent = 0, i;
    if (s == NULL || p < 0 || p >= s->player_count ||
        out_count == NULL || out_rng_after == NULL) return 0;
    player = &s->players[p];
    *out_count = player->ndices;
    *out_rng_after = rng_before;
    traffic = player->traffic_method;
    if ((traffic != 1 && traffic != 2) || player->remote_dice_value != 0 ||
        player->days_in_prison || player->days_in_hospital ||
        player->days_sleeping || player->days_sleep_walking ||
        player->days_disappearing ||
        (player->play_state & 0x30) != 0) return 1;
    *out_count = traffic + 1;
    if (player->attached_timed_bomb_slot >= 0 &&
        player->attached_timed_bomb_slot < GAME_TOOL_PLACED_OBJECT_COUNT) {
        const struct GameToolPlacedObjectState *bomb =
            &s->tool_placed_objects[player->attached_timed_bomb_slot];
        if (bomb->kind == 0x12 && bomb->attached_player == p + 1) {
            if (bomb->countdown < 15) *out_count = 1;
            return 1;
        }
    }
    rng.state = rng_before;
    count = ai_collect_original_route_window(
        s, player->node, player->previous_node, 5, &rng, nodes, NULL);
    for (i = 0; i < count; i++) {
        uint16_t raw = s->route_object_raw[nodes[i]];
        int owner;
        if (raw > 0x7d0 && raw < 0xfa0 &&
            (int)raw - 0x7d1 < s->land_count)
            owner = s->land_owner[(int)raw - 0x7d1];
        else if (raw > 0xfa0 && raw < 0x1770 &&
                 (int)raw - 0xfa1 < s->facility_count)
            owner = s->facility_owner[(int)raw - 0xfa1];
        else continue;
        if (owner < 0 || owner == p) useful++;
        else opponent++;
    }
    if (useful == 0 && opponent > 2 && traffic == 2)
        *out_count = (rich4_rng_original_state_next(&rng) & 1) + 2;
    if (useful >= 2 && opponent <= 1) *out_count = 1;
    *out_rng_after = rng.state;
    return 1;
}

int rich4_ai_select_command_with_selection_memory(
    const GameState *state, GameDomainRef current_property,
    const uint8_t *player_on_map,
    const GameAiMapTargetContext *target_context,
    const GameAiGlobalProjection *projection,
    const GameAiSelectionMemory *selection_before, int player,
    GameAiPhase phase, uint32_t rng_state, GameAiDecision *out)
{
    Rich4Rng rng;
    uint8_t projected_player_on_map[GAME_MAX_PLAYERS];
    const uint8_t *effective_player_on_map = player_on_map;
    GameDomainRef effective_current_property = current_property;
    if (out == NULL || !active_ai(state, player) || phase < GAME_AI_PHASE_STOCK_BUY ||
        phase > GAME_AI_PHASE_DONE) return 0;
    memset(out, 0, sizeof(*out));
    if (selection_before != NULL) out->selection_after = *selection_before;
    if (phase == GAME_AI_PHASE_STOCK_BUY)
        out->selection_after.stock_distress = 0;
    out->phase = phase;
    out->next_phase = phase < GAME_AI_PHASE_DONE ? (GameAiPhase)(phase + 1) :
                                                   GAME_AI_PHASE_DONE;
    out->player = player;
    out->personality = (GameAiPersonality)state->players[player].ai_personality;
    if (out->personality < GAME_AI_GOOD_CHILD ||
        out->personality >= GAME_AI_PERSONALITY_COUNT) return 0;
    out->rng_before = rng_state;
    rng.state = rng_state;
    if (phase == GAME_AI_PHASE_CARD_OR_TOOL && projection != NULL &&
        projection->object_table_available) {
        (void)ai_project_visible_player_mask(
            state, projection, projected_player_on_map);
        /* A supplied raw object table is authoritative. A malformed table
         * deliberately leaves the projected set empty rather than reviving
         * players from route-only state. */
        effective_player_on_map = projected_player_on_map;
    } else if (effective_player_on_map == NULL &&
        ai_project_player_map(state, projected_player_on_map))
        effective_player_on_map = projected_player_on_map;
    if (rich4_game_domain_ref_is_none(effective_current_property))
        effective_current_property = ai_project_current_property(state, player);
    switch (phase) {
    case GAME_AI_PHASE_STOCK_BUY: rich4_ai_select_stock_buy(state, player, &rng, out); break;
    case GAME_AI_PHASE_STOCK_SELL: rich4_ai_select_stock_sell(state, player, &rng, out); break;
    case GAME_AI_PHASE_FUNDS:
        out->command = rich4_game_command(
            GAME_COMMAND_SETTLE_AI_SPECIAL_FINANCE,
            GAME_COMMAND_SOURCE_AI, player, 0);
        out->has_command = 1;
        trace(out, none_ref(), none_ref(), GAME_AI_CANDIDATE_SELECTED, 0, 0);
        break;
    case GAME_AI_PHASE_CARD_OR_TOOL:
        if ((rich4_rng_original_state_next(&rng) & 1) != 0)
            select_card(state, effective_current_property,
                        effective_player_on_map,
                        target_context, projection, player, &rng, out);
        else
            select_tool(state, player, &rng, projection, out);
        break;
    case GAME_AI_PHASE_PROPERTY_UPGRADE: select_upgrade(state, player, out); break;
    case GAME_AI_PHASE_MOVE:
        out->command = rich4_game_command_roll_dice(
            GAME_COMMAND_SOURCE_AI, player);
        out->has_command = 1;
        trace(out, none_ref(), none_ref(), GAME_AI_CANDIDATE_SELECTED, 0, 0);
        break;
    case GAME_AI_PHASE_DONE: break;
    }
    out->rng_after = rng.state;
    return 1;
}

int rich4_ai_select_command_with_map_context_and_projection(
    const GameState *state, GameDomainRef current_property,
    const uint8_t *player_on_map,
    const GameAiMapTargetContext *target_context,
    const GameAiGlobalProjection *projection, int player,
    GameAiPhase phase, uint32_t rng_state, GameAiDecision *out)
{
    return rich4_ai_select_command_with_selection_memory(
        state, current_property, player_on_map, target_context, projection,
        NULL, player, phase, rng_state, out);
}

int rich4_ai_select_command_with_map_context(
    const GameState *state, GameDomainRef current_property,
    const uint8_t *player_on_map,
    const GameAiMapTargetContext *target_context, int player,
    GameAiPhase phase, uint32_t rng_state, GameAiDecision *out)
{
    return rich4_ai_select_command_with_map_context_and_projection(
        state, current_property, player_on_map, target_context, NULL, player,
        phase, rng_state, out);
}

int rich4_ai_select_command_with_map(
    const GameState *state, GameDomainRef current_property,
    const uint8_t *player_on_map, int player,
    GameAiPhase phase, uint32_t rng_state, GameAiDecision *out)
{
    return rich4_ai_select_command_with_map_context(
        state, current_property, player_on_map, NULL, player, phase,
        rng_state, out);
}

int rich4_ai_select_command(const GameState *state, int player,
                            GameAiPhase phase, uint32_t rng_state,
                            GameAiDecision *out)
{
    return rich4_ai_select_command_with_map(
        state, none_ref(), NULL, player, phase, rng_state, out);
}

const char *rich4_ai_personality_name(GameAiPersonality personality)
{
    static const char *names[GAME_AI_PERSONALITY_COUNT] = {
        "乖宝宝", "普通人", "大老奸"
    };
    return personality >= GAME_AI_GOOD_CHILD && personality < GAME_AI_PERSONALITY_COUNT ?
           names[personality] : "";
}
