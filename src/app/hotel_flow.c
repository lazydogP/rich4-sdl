#include "app/hotel_flow.h"

#include <string.h>

#include "app/route_geometry_flow.h"
#include "core/game_event_payload.h"
#include "view/game_view_model.h"

int rich4_hotel_anchor(const GameState *state, const MapData *map, int player,
                        int *x, int *y)
{
    int node, raw, facility;
    if (state == NULL || map == NULL || map->routes == NULL ||
        map->facilities == NULL || player < 0 || player >= state->player_count)
        return 0;
    node = state->players[player].node;
    if (node <= 0 || (size_t)node >= map->route_count) return 0;
    raw = map->routes[node].object.raw_object_id;
    facility = raw - 4000;
    if (facility <= 0 || (size_t)facility >= map->facility_count) return 0;
    if (x != NULL) *x = map->facilities[facility].x;
    if (y != NULL) *y = map->facilities[facility].y;
    return 1;
}

int rich4_hotel_capture_entry(GameSession *session, const GameEvent *event)
{
    Rich4GameEventFields fields;
    GameHotelEntryPresentationState *entry;
    if (session == NULL || event == NULL || event->type != GAME_EVENT_STATUS_CHANGED)
        return 0;
    fields = rich4_game_event_status_changed_fields(event);
    if (fields.subject_ref.kind != GAME_DOMAIN_STATUS ||
        fields.subject_ref.id != GAME_STATUS_DELAYED_BLOCK ||
        fields.target_ref.kind != GAME_DOMAIN_FACILITY || fields.value == 0 ||
        event->player < 0 || event->player >= session->state.player_count)
        return 0;
    entry = &session->presentation.hotel_entry;
    memset(entry, 0, sizeof(*entry));
    entry->pending = 1;
    entry->player = event->player;
    entry->facility = fields.target_ref.id;
    return 1;
}

int rich4_hotel_update_entry(GameSession *session, const MapData *map,
                              uint32_t now, int frame_ms)
{
    GameHotelEntryPresentationState *entry;
    GameActorPresentationState *actor;
    GamePlayerState *player;
    float duration;
    int remaining;
    if (session == NULL || frame_ms <= 0) return 0;
    entry = &session->presentation.hotel_entry;
    if (!entry->pending && !entry->active) return 0;
    if (entry->player < 0 || entry->player >= session->state.player_count ||
        map == NULL || map->facilities == NULL || entry->facility < 0 ||
        (size_t)(entry->facility + 1) >= map->facility_count)
        return 1;
    player = &session->state.players[entry->player];
    actor = &session->presentation.actors[entry->player];
    if (entry->pending) {
        const MapFacilityRecord *facility = &map->facilities[entry->facility + 1];
        entry->pending = 0;
        if (entry->player != session->state.current_player ||
            (player->play_state & GAME_PLAYER_PLAY_STATE_DELAYED_BLOCK_VISIBLE) == 0) {
            /* 0x40D650: substitute payers relocate without entering EA2=1. */
            actor->visual_x = facility->x;
            actor->visual_y = facility->y;
            actor->move_active = 0;
            actor->direction = session->presentation.actors[
                session->state.current_player].direction;
            memset(entry, 0, sizeof(*entry));
            return 0;
        }
        entry->active = 1;
        entry->phase = 1;
        entry->saved_direction = actor->direction;
        entry->tick = session->presentation.movement_tick != 0
            ? session->presentation.movement_tick : now;
        actor->move_from_node = actor->move_to_node = player->node;
        rich4_route_node_xy_for_map(map, player->node,
                                    &actor->move_from_x, &actor->move_from_y);
        actor->move_to_x = facility->x;
        actor->move_to_y = facility->y;
        actor->move_progress = 0;
        duration = rich4_original_fixed_move_duration(
            actor->move_to_x - actor->move_from_x,
            actor->move_to_y - actor->move_from_y);
        actor->move_distance = (int)duration;
        if (actor->move_distance < 1) actor->move_distance = 1;
        actor->direction = rich4_game_actor_direction_from_delta(
            facility->x - actor->visual_x, facility->y - actor->visual_y);
        actor->walk_frame = 0;
        actor->move_active = 1;
        session->presentation.has_view_override = 0;
    }
    if (!rich4_consume_animation_pulse(&entry->tick, now, frame_ms)) return 1;
    if (entry->phase == 2) {
        /* Next EA2=1 pulse stops audio, then five idle and three blocked ticks. */
        entry->phase = 3;
        entry->wait_ticks = 8;
        return 1;
    }
    if (entry->phase == 3) {
        if (--entry->wait_ticks > 0) return 1;
        player->play_state &= 0x0f;
        actor->direction = entry->saved_direction;
        memset(entry, 0, sizeof(*entry));
        return 2;
    }
    duration = rich4_original_fixed_move_duration(
        actor->move_to_x - actor->move_from_x,
        actor->move_to_y - actor->move_from_y);
    actor->move_progress++;
    actor->walk_frame++;
    remaining = actor->move_distance - actor->move_progress;
    if (remaining <= 0) {
        actor->visual_x = actor->move_to_x;
        actor->visual_y = actor->move_to_y;
        actor->move_active = 0;
        entry->phase = 2;
        return 1;
    }
    actor->visual_x = rich4_original_interpolated_axis(
        actor->move_from_x, actor->move_to_x, actor->move_progress, duration);
    actor->visual_y = rich4_original_interpolated_axis(
        actor->move_from_y, actor->move_to_y, actor->move_progress, duration);
    if (remaining < (actor->move_distance >> 1))
        player->play_state &= 0x0f;
    return 1;
}
