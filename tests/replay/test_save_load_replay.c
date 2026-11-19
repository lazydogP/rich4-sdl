#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/ai_flow.h"
#include "app/audio_event.h"
#include "app/dynamic_object_flow.h"
#include "app/game_save.h"
#include "app/game_session.h"
#include "app/game_presentation.h"
#include "core/card.h"
#include "core/game_command.h"
#include "core/game_dispatch.h"
#include "core/game_event_payload.h"
#include "core/game_flow_state.h"
#include "core/tool.h"
#include "core/rng.h"
#include "core/special_event.h"
#include "data/character_speech.h"
#include "save_v5_fixture.h"

#if defined(_WIN32)
#include <process.h>
#include <direct.h>
#define rmdir _rmdir
#define getpid _getpid
#else
#include <unistd.h>
#endif

enum { REPLAY_TEXT_CAPACITY = 32768 };

typedef struct AiReplaySink {
    Rich4AiCommandSink api;
    GameSession *session;
    char text[REPLAY_TEXT_CAPACITY];
    size_t used;
    int create_count;
    int reprice_count;
    int buy_count;
} AiReplaySink;

static GameSession sessions[5];
static GameSession sentinel;
static GameSession sentinel_before;
static MapRouteNode map_routes[3];
static MapRouteNode large_map_routes[104];
static MapLandRecord map_lands[3];
static MapFacilityRecord map_facilities[3];
static MapAuxiliaryRecord map_auxiliary[3];

static int append_text(char *text, size_t capacity, size_t *used,
                       const char *format, ...)
{
    va_list args;
    int written;
    if (text == NULL || used == NULL || format == NULL || *used >= capacity)
        return 0;
    va_start(args, format);
    written = vsnprintf(text + *used, capacity - *used, format, args);
    va_end(args);
    if (written < 0 || (size_t)written >= capacity - *used) return 0;
    *used += (size_t)written;
    return 1;
}

static void capture_speech(GamePresentationState *presentation,
                           const char *current_text,
                           const char *last_text)
{
    GameSpeechPresentationState *speech = &presentation->speech;
    assert(rich4_character_speech_snapshot_capture_text(
        &speech->current_snapshot, current_text));
    assert(rich4_character_speech_snapshot_restore(
        &speech->current_snapshot, &speech->restored_current));
    speech->current = &speech->restored_current;
    speech->active = 1;
    assert(rich4_character_speech_snapshot_capture_text(
        &speech->last_text_snapshot, last_text));
    speech->last_text = speech->last_text_snapshot.big5;
    speech->last_text_rehydrated = 1;
}

static void initialize_session(GameSession *session)
{
    int i;
    memset(session, 0, sizeof(*session));
    session->state.active = 1;
    session->state.player_count = 2;
    session->state.current_player = 0;
    session->state.stage = 0;
    session->state.map = 0;
    session->state.price_index = 1;
    session->state.tool_route_node_count = 2;
    session->state.land_count = 1;
    session->state.facility_count = 1;
    session->state.company_count = 1;
    session->state.players[0].play_state = 2;
    session->state.players[0].is_ai = 1;
    session->state.players[0].cash = 20000;
    session->state.players[0].bank = 5000;
    session->state.players[0].node = 1;
    session->state.players[0].previous_node = 0;
    session->state.players[0].next_node = 1;
    session->state.players[1].play_state = 2;
    session->state.players[1].cash = 20000;
    session->state.players[1].bank = 5000;
    session->state.players[1].node = 0;
    session->state.players[1].previous_node = 1;
    session->state.players[1].next_node = 1;
    session->state.land_owner[0] = -1;
    session->state.facility_owner[0] = -1;
    session->state.company_owner[0] = -1;
    session->state.elapsed_days = 5;
    session->state.full_round_count = 5;
    session->state.stock_market_total_x10 = 1000;
    session->state.ai_rng_state = 1;
    session->map_access_flags[0] = 1;
    session->map_access_flags[2] = 1;
    for (i = 0; i < GAME_STOCK_COUNT; i++) {
        session->state.stocks[i].f16 = 100.0f;
        session->state.stocks[i].f20 = 100.0f;
    }
    session->flow.turn_state = GAME_TURN_STATE_IDLE;
    rich4_presentation_initialize(&session->presentation);
    session->presentation.mode = RICH4_PRESENTATION_SPEECH;
    capture_speech(&session->presentation, "current voice snapshot",
                   "last spoken line");

    session->time_machine_valid = 1;
    session->state.land_lease_date[0] = 0x07d00c0fu;
    session->time_machine_state = session->state;
    session->time_machine_state.land_lease_date[0] = 0x07d00b0fu;
    session->time_machine_state.elapsed_days = 4;
    session->time_machine_flow = session->flow;
    rich4_presentation_initialize(&session->time_machine_presentation);
    capture_speech(&session->time_machine_presentation,
                   "rewind voice snapshot", "rewind last line");
    rich4_audio_event_queue_init(&session->audio_events);
    assert(rich4_audio_event_emit(&session->audio_events,
        RICH4_AUDIO_EVENT_OK, 1, 77));
}

static void initialize_map(MapData *map)
{
    memset(map_routes, 0, sizeof(map_routes));
    memset(map_lands, 0, sizeof(map_lands));
    memset(map_facilities, 0, sizeof(map_facilities));
    memset(map_auxiliary, 0, sizeof(map_auxiliary));
    memset(map, 0, sizeof(*map));
    map->routes = map_routes;
    map->route_count = 2;
    map->lands = map_lands;
    map->land_count = 2;
    map->facilities = map_facilities;
    map->facility_count = 2;
    map->auxiliary = map_auxiliary;
    map->auxiliary_count = 2;
}

static int file_equals(const char *left_path, const char *right_path)
{
    FILE *left = fopen(left_path, "rb");
    FILE *right = fopen(right_path, "rb");
    int equal = 0;
    if (left != NULL && right != NULL) {
        unsigned char left_buffer[4096];
        unsigned char right_buffer[4096];
        size_t left_count;
        size_t right_count;
        equal = 1;
        do {
            left_count = fread(left_buffer, 1, sizeof(left_buffer), left);
            right_count = fread(right_buffer, 1, sizeof(right_buffer), right);
            if (left_count != right_count ||
                memcmp(left_buffer, right_buffer, left_count) != 0) {
                equal = 0;
                break;
            }
        } while (left_count != 0);
        if (ferror(left) || ferror(right)) equal = 0;
    }
    if (left != NULL) fclose(left);
    if (right != NULL) fclose(right);
    return equal;
}

static uint32_t lease_fixture_u32(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void lease_fixture_put_u32(unsigned char *p, uint32_t value)
{
    for (int i = 0; i < 4; i++) p[i] = (unsigned char)(value >> (8 * i));
}

static void test_tool_animation_round_trip(const char *directory)
{
    static GameSession source, loaded;
    char path[512];
    initialize_session(&source);
    assert(snprintf(path, sizeof(path), "%s/tool-animation.R4S", directory) > 0);
    source.missile_scene_active = 1;
    source.missile_scene.world_x = 700;
    source.missile_scene.world_y = 1000;
    source.missile_scene.lands[0] = (GameMissileBuilding){-1, 1, 1};
    source.missile_scene.facilities[0] = (GameMissileBuilding){0, 4, 5};
    source.missile_scene.objects[17].node = 1;
    source.presentation.tool_animation_count = 1;
    source.presentation.tool_animation[0].active = 1;
    source.presentation.tool_animation[0].resource = 530;
    source.presentation.tool_animation[0].after_sync_player = -1;
    source.presentation.tool_animation[0].after_speech_player = -1;
    source.presentation.tool_animation[0].after_speech_next_player = -1;
    source.presentation.tool_animation[0].frame = 7;
    assert(rich4_game_save_write(path, 2, &source));
    assert(rich4_game_save_read(path, 2, &loaded));
    assert(loaded.missile_scene_active && loaded.presentation.tool_animation[0].frame == 7);
    assert(loaded.missile_scene.world_x == 700 && loaded.missile_scene.world_y == 1000);
    assert(loaded.missile_scene.lands[0].owner == -1 && loaded.missile_scene.lands[0].type == 1);
    assert(loaded.missile_scene.facilities[0].level == 5 && loaded.missile_scene.objects[17].node == 1);
    initialize_session(&source);
    source.state.players[0].tool_inventory[2] = 1;
    GameToolPlacementAnimation *flight = &source.presentation.tool_placement_animation;
    flight->active = 1;
    flight->tool = 2;
    flight->slot = 10;
    flight->debit_player_plus_one = 1;
    flight->frame = 3;
    flight->frame_count = 7;
    assert(rich4_game_save_write(path, 2, &source));
    assert(rich4_game_save_read(path, 2, &loaded));
    flight = &loaded.presentation.tool_placement_animation;
    assert(flight->frame == 3 && flight->frame_count == 7 && flight->debit_player_plus_one == 1);
    assert(remove(path) == 0);
}

static void test_format_10_lease_compatibility(const char *directory)
{
    static GameSession source, loaded;
    char path[512];
    unsigned char *bytes;
    FILE *file;
    long length;
    size_t at, size;
    size_t additional_start = 0;
    size_t legacy_elapsed_start = 0;
    size_t suffix_start = 0;
    size_t direction_field_size = GAME_SPECIAL_PLAYER_COUNT * 4u;
    size_t lease_field_size = GAME_MAX_MAP_PROPERTIES * 4u;
    uint32_t crc = 0xffffffffu;
    int directions_removed = 0;
    int removed = 0;
    initialize_session(&source);
    source.time_machine_state.current_player = 0;
    source.time_machine_state.elapsed_days = 0x13572466;
    source.time_machine_other[0].state.active = 0x13572468;
    source.bankruptcy_auction_events.events[0].type =
        (GameEventType)0x24681357;
    source.state.land_period_index = source.time_machine_state.land_period_index = 5;
    source.state.land_runtime_value[0] = 123456;
    source.state.facility_last_change[0] = 0x07d00701u;
    for (int i = 0; i < GAME_MAX_MAP_PROPERTIES; i++) {
        source.state.land_lease_date[i] = 0x07d00601u;
        source.time_machine_state.land_lease_date[i] = 0x07d00501u;
        source.presentation.special_event.pre_state.land_lease_date[i] = 0x07d00601u;
        source.time_machine_presentation.special_event.pre_state.land_lease_date[i] = 0x07d00501u;
    }
    for (int i = 0; i < GAME_SPECIAL_PLAYER_COUNT; i++) {
        source.state.special_player_direction[i] = 0x01234567;
        source.time_machine_state.special_player_direction[i] = 0x07654321;
        source.presentation.special_event.pre_state.special_player_direction[i] =
            0x01234567;
        source.time_machine_presentation.special_event.pre_state
            .special_player_direction[i] = 0x07654321;
    }
    assert(snprintf(path, sizeof(path), "%s/lease-format-10.R4S", directory) > 0);
    assert(rich4_game_save_write(path, 2, &source));
    file = fopen(path, "rb");
    assert(file != NULL && fseek(file, 0, SEEK_END) == 0);
    length = ftell(file);
    assert(length > 20 && fseek(file, 0, SEEK_SET) == 0);
    size = (size_t)length;
    bytes = malloc(size);
    assert(bytes != NULL && fread(bytes, 1, size, file) == size);
    assert(fclose(file) == 0 && lease_fixture_u32(bytes + 4) == 13);
    /* Drop format 13's tool continuation trailer (no active missile). */
    size -= (2 * 6 + 1) * 4;
    for (at = 20; at + 4 <= size; at++) {
        uint32_t marker = lease_fixture_u32(bytes + at);
        if (marker == 0x13572466u) legacy_elapsed_start = at;
        if (marker == 0x13572468u) additional_start = at;
        if (marker == 0x24681357u) {
            suffix_start = at;
            break;
        }
    }
    assert(legacy_elapsed_start > 40 &&
           additional_start > legacy_elapsed_start &&
           suffix_start > additional_start);
    memmove(bytes + additional_start, bytes + suffix_start,
            size - suffix_start);
    size -= suffix_start - additional_start;
    /* Formats through 11 stored one global rewind snapshot and identified its
     * owner through GameState.current_player.  Format 12 assigns that legacy
     * slot to player zero, so patch the already-encoded old-layout fixture to
     * exercise migration of a player-one snapshot. current_player is five
     * serialized i32 fields before elapsed_days in GameState. */
    lease_fixture_put_u32(bytes + legacy_elapsed_start - 5u * 4u, 1);
    /* Format 12 also adds the per-special-actor direction array to the same
     * four GameState snapshots. Remove those to reach the format-11 layout. */
    for (at = 20; at + direction_field_size <= size; at++) {
        uint32_t marker = lease_fixture_u32(bytes + at);
        size_t i;
        if (marker != 0x01234567u && marker != 0x07654321u) continue;
        for (i = 4; i < direction_field_size; i += 4)
            if (lease_fixture_u32(bytes + at + i) != marker) break;
        if (i != direction_field_size) continue;
        memmove(bytes + at, bytes + at + direction_field_size,
                size - at - direction_field_size);
        size -= direction_field_size;
        directions_removed++;
    }
    assert(directions_removed == 4);
    /* Format 11 adds these arrays to live/rewind state and their event snapshots.
     * Distinct repeated dates locate them without coupling the test to the
     * offsets of unrelated save fields. Remove them to produce format 10. */
    for (at = 20; at + lease_field_size <= size; at++) {
        uint32_t marker = lease_fixture_u32(bytes + at);
        size_t i;
        if (marker != 0x07d00601u && marker != 0x07d00501u) continue;
        for (i = 4; i < lease_field_size; i += 4)
            if (lease_fixture_u32(bytes + at + i) != marker) break;
        if (i != lease_field_size) continue;
        memmove(bytes + at, bytes + at + lease_field_size,
                size - at - lease_field_size);
        size -= lease_field_size;
        removed++;
    }
    assert(removed == 4);
    lease_fixture_put_u32(bytes + 4, 10);
    lease_fixture_put_u32(bytes + 12, (uint32_t)(size - 20));
    for (at = 20; at < size; at++) {
        crc ^= bytes[at];
        for (int bit = 0; bit < 8; bit++)
            crc = (crc >> 1) ^ ((crc & 1u) ? 0xedb88320u : 0);
    }
    lease_fixture_put_u32(bytes + 16, ~crc);
    file = fopen(path, "wb");
    assert(file != NULL && fwrite(bytes, 1, size, file) == size);
    assert(fclose(file) == 0);
    free(bytes);
    assert(rich4_game_save_read(path, 2, &loaded));
    assert(loaded.state.land_period_index == 5 &&
           !loaded.time_machine_valid &&
           loaded.time_machine_other[0].valid &&
           loaded.time_machine_other[0].state.land_period_index == 5);
    assert(loaded.state.land_runtime_value[0] == 123456);
    assert(loaded.state.facility_last_change[0] == 0x07d00701u);
    for (int i = 0; i < GAME_MAX_MAP_PROPERTIES; i++)
        assert(loaded.state.land_lease_date[i] == 0 &&
               loaded.time_machine_other[0].state.land_lease_date[i] == 0);
    assert(remove(path) == 0);
}

static int write_variant(const char *source_path, const char *target_path,
                         int variant)
{
    FILE *source = fopen(source_path, "rb");
    FILE *target;
    unsigned char *bytes;
    long file_size;
    size_t output_size;
    int ok;
    if (source == NULL || fseek(source, 0, SEEK_END) != 0 ||
        (file_size = ftell(source)) < 20 ||
        fseek(source, 0, SEEK_SET) != 0) {
        if (source != NULL) fclose(source);
        return 0;
    }
    bytes = (unsigned char *)malloc((size_t)file_size);
    if (bytes == NULL) {
        fclose(source);
        return 0;
    }
    ok = fread(bytes, 1, (size_t)file_size, source) == (size_t)file_size;
    fclose(source);
    if (!ok) {
        free(bytes);
        return 0;
    }
    output_size = (size_t)file_size;
    if (variant == 0) {
        bytes[4] = 2;
        bytes[5] = bytes[6] = bytes[7] = 0;
    } else if (variant == 1) {
        bytes[output_size - 1] ^= 0x80;
    } else if (variant == 2) {
        output_size--;
    } else {
        free(bytes);
        return 0;
    }
    target = fopen(target_path, "wb");
    if (target == NULL) {
        free(bytes);
        return 0;
    }
    ok = fwrite(bytes, 1, output_size, target) == output_size;
    if (fclose(target) != 0) ok = 0;
    free(bytes);
    return ok;
}

static void assert_failed_read_is_atomic(const char *path, int slot)
{
    memset(&sentinel, 0xa5, sizeof(sentinel));
    sentinel.state.system_cash = 0x1234567;
    sentinel.state.elapsed_days = 0x7654321;
    sentinel_before = sentinel;
    assert(!rich4_game_save_read(path, slot, &sentinel));
    assert(memcmp(&sentinel_before, &sentinel, sizeof(sentinel)) == 0);
}

static void round_trip_case(const char *directory, const char *name,
                           const GameSession *source, int slot,
                           const MapData *map)
{
    char path[512];
    char copy_path[512];
    GameSession *loaded = &sessions[0];
    MapData mismatched = *map;
    assert(snprintf(path, sizeof(path), "%s/%s.R4S", directory, name) > 0);
    assert(snprintf(copy_path, sizeof(copy_path), "%s/%s-copy.R4S",
                    directory, name) > 0);
    assert(rich4_game_save_write(path, slot, source));
    memset(loaded, 0, sizeof(*loaded));
    assert(rich4_game_save_read(path, slot, loaded));
    assert(memcmp(&loaded->state, &source->state,
                  sizeof(loaded->state)) == 0);
    assert(memcmp(&loaded->flow, &source->flow,
                  sizeof(loaded->flow)) == 0);
    assert(memcmp(&loaded->time_machine_state, &source->time_machine_state,
                  sizeof(loaded->time_machine_state)) == 0);
    assert(memcmp(&loaded->time_machine_flow, &source->time_machine_flow,
                  sizeof(loaded->time_machine_flow)) == 0);
    assert(memcmp(loaded->map_access_flags, source->map_access_flags,
                  sizeof(loaded->map_access_flags)) == 0);
    assert(loaded->time_machine_other[0].valid ==
           source->time_machine_other[0].valid);
    assert(memcmp(&loaded->time_machine_other[0].state,
                  &source->time_machine_other[0].state,
                  sizeof(loaded->time_machine_other[0].state)) == 0);
    assert(memcmp(&loaded->time_machine_other[0].flow,
                  &source->time_machine_other[0].flow,
                  sizeof(loaded->time_machine_other[0].flow)) == 0);
    assert(memcmp(loaded->time_machine_other[0].actors,
                  source->time_machine_other[0].actors,
                  sizeof(loaded->time_machine_other[0].actors)) == 0);
    assert(memcmp(loaded->time_machine_other[0].dynamic_objects,
                  source->time_machine_other[0].dynamic_objects,
                  sizeof(loaded->time_machine_other[0].dynamic_objects)) == 0);
    assert(loaded->audio_events.count == 0);
    assert(loaded->presentation.speech.current ==
           &loaded->presentation.speech.restored_current);
    assert(loaded->presentation.speech.current !=
           source->presentation.speech.current);
    assert(strcmp(loaded->presentation.speech.current->big5,
                  "current voice snapshot") == 0);
    assert(loaded->presentation.speech.last_text ==
           loaded->presentation.speech.last_text_snapshot.big5);
    assert(loaded->time_machine_presentation.speech.current ==
           &loaded->time_machine_presentation.speech.restored_current);
    assert(loaded->state.stage == source->state.stage);
    assert(loaded->state.map == source->state.map);
    assert(loaded->time_machine_state.stage ==
           source->time_machine_state.stage);
    assert(loaded->time_machine_state.map == source->time_machine_state.map);
    assert(rich4_game_save_write(copy_path, slot, loaded));
    assert(file_equals(path, copy_path));
    assert(rich4_game_save_session_matches_map(loaded, map));
    mismatched.land_count++;
    assert(!rich4_game_save_session_matches_map(loaded, &mismatched));
    remove(path);
    remove(copy_path);
}

static void test_attached_bomb_roundtrip(const char *directory,
                                         const MapData *map)
{
    static GameSession source;
    int mask;
    /* 0x41BFD2 keeps the bomb slot and binds it to a player while clearing
     * its route association. Cover both the live state and time-machine copy. */
    for (mask = 1; mask <= 3; mask++) {
        GameState *states[2];
        struct GameToolPlacedObjectState *bomb;
        int p, i;
        initialize_session(&source);
        states[0] = &source.state;
        states[1] = &source.time_machine_state;
        for (p = 0; p < 2; p++) {
            GameEventQueue events;
            for (i = 0; i < GAME_MAX_PLAYERS; i++)
                states[p]->players[i].attached_timed_bomb_slot = -1;
            if (!(mask & (1 << p))) continue;
            bomb = &states[p]->tool_placed_objects[20];
            bomb->kind = 0x12;
            bomb->node = 1;
            bomb->owner = 0;
            bomb->attached_player = 0xff;
            rich4_game_event_queue_reset(&events);
            assert(rich4_tool_trigger_at_node(states[p], 1, 1, 1, &events) ==
                   GAME_TOOL_TRIGGER_PASS);
            assert(bomb->node == 0 && bomb->attached_player == 2 &&
                   states[p]->players[1].attached_timed_bomb_slot == 20);
        }
        round_trip_case(directory, "attached-bomb", &source, 2, map);
        bomb = &states[mask & 1 ? 0 : 1]->tool_placed_objects[20];
        bomb->kind = 0x10;
        assert(!rich4_game_save_session_matches_map(&source, map));
        bomb->kind = 0x12;
        bomb->attached_player = 0;
        assert(!rich4_game_save_session_matches_map(&source, map));
        bomb->attached_player = 0xff;
        assert(!rich4_game_save_session_matches_map(&source, map));
        bomb->attached_player = source.state.player_count + 1;
        assert(!rich4_game_save_session_matches_map(&source, map));
        bomb->attached_player = 1; /* No matching player backlink. */
        assert(!rich4_game_save_session_matches_map(&source, map));
        bomb->attached_player = 2;
        bomb->node = -1;
        assert(!rich4_game_save_session_matches_map(&source, map));
        bomb->node = (int)map->route_count;
        assert(!rich4_game_save_session_matches_map(&source, map));
    }
}

static int ai_replay_submit(Rich4AiCommandSink *sink,
                            const GameCommand *command, uint32_t now)
{
    AiReplaySink *replay = (AiReplaySink *)sink;
    GameEventQueue events;
    GameEvent event;
    int accepted;
    (void)now;
    rich4_game_event_queue_reset(&events);
    accepted = rich4_game_dispatch_command(
        &replay->session->state, &replay->session->flow,
        command, &events);
    if (accepted && command->type == GAME_COMMAND_CREATE_MARKET_LISTING)
        replay->create_count++;
    if (accepted && command->type == GAME_COMMAND_REPRICE_MARKET_LISTING)
        replay->reprice_count++;
    if (accepted && command->type == GAME_COMMAND_BUY_MARKET_LISTING)
        replay->buy_count++;
    if (!append_text(replay->text, sizeof(replay->text), &replay->used,
                     "command=%d,p=%d,subject=%d:%d,value=%d,value2=%d,ok=%d\n",
                     command->type, command->player,
                     command->subject.kind, command->subject.id,
                     command->value, command->value2, accepted))
        return 0;
    while (rich4_game_event_queue_pop(&events, &event)) {
        GameDomainRef subject = rich4_game_event_subject_ref(&event);
        GameDomainRef target = rich4_game_event_target_ref(&event);
        if (!append_text(replay->text, sizeof(replay->text), &replay->used,
                "event=%d,p=%d,value=%d,value2=%d,subject=%d:%d,target=%d:%d,reject=%d\n",
                event.type, event.player, rich4_game_event_value(&event),
                rich4_game_event_value2(&event), subject.kind, subject.id,
                target.kind, target.id, event.reject_reason))
            return 0;
    }
    return accepted;
}

static int select_card_and_tool(int *out_card_id, int *out_tool_id)
{
    int i;
    *out_card_id = -1;
    *out_tool_id = -1;
    for (i = 0; i < GAME_CARD_COUNT; i++) {
        const GameCardInfo *card = rich4_card_info(i);
        if (card != NULL && card->shop_price > 0) {
            *out_card_id = i;
            break;
        }
    }
    for (i = 0; i < GAME_TOOL_COUNT; i++) {
        const GameToolInfo *tool = rich4_tool_info(i);
        if (tool != NULL && tool->shop_price > 0) {
            *out_tool_id = i;
            break;
        }
    }
    return *out_card_id >= 0 && *out_tool_id >= 0;
}

static void initialize_market_session(GameSession *session,
                                      unsigned int seed,
                                      int card_id,
                                      int tool_id)
{
    int i;
    initialize_session(session);
    session->flow.turn_state = GAME_TURN_STATE_IDLE;
    session->state.current_player = 0;
    session->state.ai_rng_state = seed;
    session->state.players[0].cash = 20000;
    session->state.players[0].tool_inventory[tool_id] = 3;
    for (i = 0; i < GAME_PLAYER_CARD_SLOTS - 2; i++)
        session->state.players[0].card_inventory[i] =
            (uint8_t)(card_id + 1);
    session->state.market_listings[0][0].type =
        GAME_MARKET_LISTING_TOOL;
    session->state.market_listings[0][0].item_id =
        (uint16_t)(tool_id + 1);
    session->state.market_listings[0][0].ask_price = 1;
    session->state.market_listings[0][0].quantity = 0;
    session->state.market_listings[1][0].type =
        GAME_MARKET_LISTING_STOCK;
    session->state.market_listings[1][0].item_id = 0;
    session->state.market_listings[1][0].ask_price = 10;
    session->state.market_listings[1][0].quantity = 1;
    session->state.player_stock_amount[1][0] = 2;
    session->state.stocks[0].f16 = 100.0f;
    session->state.stocks[0].f20 = 1000.0f;
}

static void execute_ai_preactions(GameSession *session, AiReplaySink *replay)
{
    uint8_t player_on_map[GAME_MAX_PLAYERS] = {0};
    memset(replay, 0, sizeof(*replay));
    replay->api.submit = ai_replay_submit;
    replay->session = session;
    rich4_ai_run_preactions(session,
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), player_on_map,
        NULL, 0, 1234, &replay->api);
}

static int find_market_replay_seed(int card_id, int tool_id,
                                   unsigned int *out_seed)
{
    unsigned int seed;
    for (seed = 1; seed < 20000; seed++) {
        AiReplaySink replay;
        initialize_market_session(&sessions[1], seed, card_id, tool_id);
        execute_ai_preactions(&sessions[1], &replay);
        if (replay.create_count > 0 && replay.reprice_count > 0 &&
            replay.buy_count > 0) {
            *out_seed = seed;
            return 1;
        }
    }
    return 0;
}

static void test_ai_market_roundtrip(const char *directory,
                                    int card_id,
                                    int tool_id,
                                    unsigned int seed)
{
    GameSession *continuous = &sessions[1];
    GameSession *loaded = &sessions[2];
    AiReplaySink first;
    AiReplaySink second;
    char path[512];
    assert(snprintf(path, sizeof(path), "%s/market-replay.R4S", directory) > 0);
    initialize_market_session(continuous, seed, card_id, tool_id);
    assert(rich4_game_save_write(path, 2, continuous));
    memset(loaded, 0, sizeof(*loaded));
    assert(rich4_game_save_read(path, 2, loaded));
    execute_ai_preactions(continuous, &first);
    execute_ai_preactions(loaded, &second);
    assert(first.create_count > 0 && first.reprice_count > 0 &&
           first.buy_count > 0);
    assert(first.create_count == second.create_count &&
           first.reprice_count == second.reprice_count &&
           first.buy_count == second.buy_count);
    assert(strcmp(first.text, second.text) == 0);
    assert(memcmp(&continuous->state, &loaded->state,
                  sizeof(continuous->state)) == 0);
    assert(memcmp(&continuous->flow, &loaded->flow,
                  sizeof(continuous->flow)) == 0);
    remove(path);
}

static void test_bad_files(const char *directory, const char *source_path)
{
    static const char *names[] = {"old-version", "bad-crc", "truncated"};
    char variant_path[512];
    int i;
    for (i = 0; i < 3; i++) {
        assert(snprintf(variant_path, sizeof(variant_path), "%s/%s.R4S",
                        directory, names[i]) > 0);
        assert(write_variant(source_path, variant_path, i));
        assert_failed_read_is_atomic(variant_path, 2);
        remove(variant_path);
    }
    assert_failed_read_is_atomic(source_path, 3);
}

static void test_failed_write_preserves_slot(const char *directory, GameSession *source)
{
    char destination[512], baseline[512], temporary[520];
    snprintf(destination, sizeof(destination), "%s/write-failure.R4S", directory);
    snprintf(baseline, sizeof(baseline), "%s/write-baseline.R4S", directory);
    snprintf(temporary, sizeof(temporary), "%s.tmp", destination);
    assert(rich4_game_save_write(destination, 0, source));
    assert(rich4_game_save_write(baseline, 0, source));
    assert(rich4_game_save_ensure_directory(temporary));
    assert(!rich4_game_save_write(destination, 0, source));
    assert(file_equals(destination, baseline));
    assert(rmdir(temporary) == 0);
    assert(!rich4_game_save_write(destination, GAME_SAVE_SLOT_COUNT, source));
    assert(file_equals(destination, baseline));
    remove(destination); remove(baseline);
}

static void test_auto_save_waits_for_original_turn_boundary(void)
{
    GameSession *session = &sessions[4];

    initialize_session(session);
    session->presentation.auto_save_pending = 1;
    assert(rich4_game_save_auto_ready(session));

    /* 0x41CF67's dividend/lottery and monthly-bank windows return before
     * 0x41C84F, and only then does 0x418EBD call 0x402FD1(0). */
    session->presentation.periodic_finance.active = 1;
    assert(!rich4_game_save_auto_ready(session));
    assert(session->presentation.auto_save_pending == 1);
    session->presentation.periodic_finance.active = 0;

    session->presentation.bank_settlement.active = 1;
    assert(!rich4_game_save_auto_ready(session));
    assert(session->presentation.auto_save_pending == 1);
    session->presentation.bank_settlement.active = 0;

    session->pending_turn_events.count = 1;
    assert(!rich4_game_save_auto_ready(session));
    assert(session->presentation.auto_save_pending == 1);
    rich4_game_event_queue_reset(&session->pending_turn_events);
    assert(rich4_game_save_auto_ready(session));
    assert(rich4_presentation_take_auto_save_pending(
        &session->presentation));
    assert(!rich4_game_save_auto_ready(session));
}

static void runtime_load_reseeds_special_targets(const char *directory)
{
    GameSession *saved=&sessions[0], *decoded=&sessions[1], *runtime=&sessions[2];
    const uint32_t ticks[3]={1000u,2000u,UINT32_MAX-9u};
    const int victims[3]={2,1,1};
    const int news_stocks[3]={2,4,1};
    GameEventQueue events;
    char path[512];
    int i,j;
    initialize_session(saved);
    saved->rng_clock_offset=9000;
    saved->state.ai_rng_state=123;
    saved->state.fortune_event_cursor=17;
    for (j=0;j<GAME_FORTUNE_EVENT_COUNT;j++)
        saved->state.fortune_event_order[j]=(uint8_t)(GAME_FORTUNE_EVENT_COUNT-j-1);
    saved->state.news_event_cursor=8;
    for (j=0;j<GAME_NEWS_EVENT_COUNT;j++)
        saved->state.news_event_order[j]=(uint8_t)(GAME_NEWS_EVENT_COUNT-j-1);
    snprintf(path,sizeof(path),"%s/runtime-load.r4s",directory);
    assert(rich4_game_save_write(path,2,saved));
    assert(rich4_game_save_read(path,2,decoded));
    assert(decoded->state.ai_rng_state==123);
    for (i=0;i<3;i++) {
        memset(runtime,0,sizeof(*runtime));
        runtime->rng_clock_offset=10;
        assert(rich4_game_save_resume(runtime,decoded,ticks[i]));
        assert(runtime->loaded_music_restart_pending == 1);
        runtime->presentation.periodic_finance.active = 1;
        runtime->presentation.periodic_finance.phase = 4;
        runtime->presentation.periodic_finance.music_started = 1;
        runtime->presentation.point_minigame.active = 1;
        runtime->presentation.point_minigame.music_started = 1;
        runtime->presentation.shop_visit.active = 1;
        runtime->presentation.shop_visit.music_started = 1;
        assert(rich4_game_save_take_music_restart(runtime));
        assert(!runtime->loaded_music_restart_pending);
        assert(!runtime->presentation.periodic_finance.music_started);
        assert(!runtime->presentation.point_minigame.music_started);
        assert(!runtime->presentation.shop_visit.music_started);
        assert(!rich4_game_save_take_music_restart(runtime));
        runtime->presentation.periodic_finance.active = 0;
        runtime->presentation.point_minigame.active = 0;
        runtime->presentation.shop_visit.active = 0;
        assert(runtime->rng_clock_offset==10 && runtime->state.ai_rng_state==ticks[i]+10u);
        assert(runtime->state.fortune_event_cursor==17 &&
               !memcmp(runtime->state.fortune_event_order,saved->state.fortune_event_order,
                       GAME_FORTUNE_EVENT_COUNT));
        assert(runtime->presentation.speech.current==&runtime->presentation.speech.restored_current);
        runtime->state.player_count=4;
        for (j=1;j<4;j++) runtime->state.players[j].play_state=2;
        assert(rich4_special_event_fortune_choose_redirect(&runtime->state,0)==victims[i]);
        assert(decoded->state.ai_rng_state==123);

        /* 0x402FA1 reseeds the shared stream; 0x44B0D1 consumes its
         * first rand() for the stock while the saved news cursor stays put. */
        assert(rich4_game_save_resume(runtime,decoded,ticks[i]));
        assert(runtime->state.news_event_cursor==8 &&
               !memcmp(runtime->state.news_event_order,saved->state.news_event_order,
                       GAME_NEWS_EVENT_COUNT));
        rich4_game_event_queue_reset(&events);
        assert(rich4_special_event_begin_news(&runtime->state,0,&events)==GAME_SPECIAL_EVENT_PREPARED);
        assert(runtime->state.special_event_logical_id==27 && runtime->state.news_event_cursor==9);
        assert((runtime->state.special_event_target_mask & 0xffffu)==(unsigned int)news_stocks[i]);
        assert(runtime->state.stocks[news_stocks[i]-1].f6==15);
        assert(decoded->state.ai_rng_state==123 && decoded->state.news_event_cursor==8);
    }
    remove(path);
}

static void runtime_load_preserves_dynamic_gods(const char *directory)
{
    GameSession *saved = &sessions[0], *decoded = &sessions[1];
    GameSession *runtime = &sessions[2];
    char path[512];
    int attempt;

    initialize_session(saved);
    rich4_app_init_dynamic_object_pool(&saved->presentation);
    saved->presentation.dynamic_objects[0].node = 1;
    saved->presentation.dynamic_objects[0].world_x = 500;
    saved->presentation.dynamic_objects[4].node = 1;
    saved->presentation.dynamic_objects[4].attached_player = 1;
    saved->presentation.dynamic_objects[4].countdown = 3;
    saved->state.players[0].god_info = 5;
    saved->state.players[0].god_type = 5;
    saved->time_machine_presentation.dynamic_objects[2].type = 3;
    saved->time_machine_presentation.dynamic_objects[2].node = 1;
    snprintf(path, sizeof(path), "%s/dynamic-gods.R4S", directory);
    memset(runtime, 0, sizeof(*runtime));
    for (attempt = 0; attempt < 32; attempt++) {
        assert(rich4_game_save_write(path, 2, saved));
        assert(rich4_game_save_read(path, 2, decoded));
        assert(rich4_game_save_resume(runtime, decoded, 1000u + attempt));
        assert(memcmp(runtime->presentation.dynamic_objects,
                      saved->presentation.dynamic_objects,
                      sizeof(saved->presentation.dynamic_objects)) == 0);
        assert(memcmp(runtime->time_machine_presentation.dynamic_objects,
                      saved->time_machine_presentation.dynamic_objects,
                      sizeof(saved->time_machine_presentation.dynamic_objects)) == 0);
        assert(runtime->state.players[0].god_info == 5);
        assert(runtime->state.players[0].god_type == 5);
        *saved = *runtime;
    }
    remove(path);
}

int main(void)
{
    static const char *directory = "build/save-load-replay";
    GameSession *source = &sessions[3];
    GameSession *loaded = &sessions[0];
    MapData map;
    MapData large_map;
    int card_id;
    int tool_id;
    unsigned int market_seed;
    char damaged_source[512];
    assert(rich4_game_save_ensure_directory(directory));
    runtime_load_reseeds_special_targets(directory);
    runtime_load_preserves_dynamic_gods(directory);
    initialize_map(&map);
    test_tool_animation_round_trip(directory);
    test_format_10_lease_compatibility(directory);
    test_attached_bomb_roundtrip(directory, &map);
    initialize_session(source);
    source->time_machine_other[0].state = source->state;
    source->time_machine_other[0].state.current_player = 1;
    source->time_machine_other[0].state.elapsed_days = 3;
    source->time_machine_other[0].flow = source->flow;
    memcpy(source->time_machine_other[0].actors,
           source->presentation.actors,
           sizeof(source->time_machine_other[0].actors));
    memcpy(source->time_machine_other[0].dynamic_objects,
           source->presentation.dynamic_objects,
           sizeof(source->time_machine_other[0].dynamic_objects));
    source->time_machine_other[0].valid = 1;
    round_trip_case(directory, "per-player-rewind", source, 2, &map);
    assert(loaded->time_machine_other[0].valid);
    assert(loaded->time_machine_other[0].state.current_player == 1);
    assert(loaded->time_machine_other[0].state.elapsed_days == 3);
    memset(&source->time_machine_other[0], 0,
           sizeof(source->time_machine_other[0]));
    test_auto_save_waits_for_original_turn_boundary();
    test_failed_write_preserves_slot(directory, source);

    /* 0x407F83 clears EA4 on load. These render-only counters do not
     * extend the save format or replace either stored body-animation frame. */
    source->presentation.sleepwalking_frame[0] = 5;
    source->presentation.sleepwalking_frame[4] = 3;
    round_trip_case(directory, "sleepwalking-render-reset", source, 2, &map);
    assert(loaded->presentation.sleepwalking_frame[0] == 0);
    assert(loaded->presentation.sleepwalking_frame[4] == 0);
    memset(source->presentation.sleepwalking_frame, 0,
           sizeof(source->presentation.sleepwalking_frame));

    source->state.special_player_actor_type[0] = 0x81;
    source->presentation.special_actor.active = 1;
    source->presentation.special_actor.path_count = 2;
    source->presentation.special_actor.path_index = 1;
    source->presentation.special_actor.path_nodes[0] = 1;
    source->presentation.special_actor.path_nodes[1] = 1;
    source->presentation.special_actor.arrival_pending = 1;
    round_trip_case(directory, "special-arrival", source, 2, &map);
    assert(loaded->presentation.special_actor.arrival_pending == 1);
    source->presentation.special_actor.arrival_pending = 0;
    source->presentation.special_actor.end_ticks_remaining = 3;
    source->time_machine_presentation.special_actor = source->presentation.special_actor;
    round_trip_case(directory, "special-end-pause", source, 2, &map);
    assert(loaded->presentation.special_actor.end_ticks_remaining == 3);
    assert(loaded->time_machine_presentation.special_actor.end_ticks_remaining == 3);
    memset(&source->presentation.special_actor, 0, sizeof(source->presentation.special_actor));
    memset(&source->time_machine_presentation.special_actor, 0,
        sizeof(source->time_machine_presentation.special_actor));
    source->state.special_player_actor_type[0] = 0;

    source->presentation.route_interaction.active = 1;
    source->presentation.route_interaction.accept =
        rich4_game_command_confirm(GAME_COMMAND_SOURCE_HUMAN, 0);
    source->presentation.route_interaction.decline =
        rich4_game_command_cancel(GAME_COMMAND_SOURCE_HUMAN, 0);
    source->flow.turn_state = GAME_TURN_STATE_ROUTE_EVENTS;
    round_trip_case(directory, "interaction", source, 2, &map);
    source->presentation.point_minigame.active = 1;
    source->presentation.point_minigame.action = 8;
    source->presentation.point_minigame.phase = RICH4_POINT_MINIGAME_PLAY;
    source->presentation.point_minigame.catcher.stage = 1;
    source->presentation.point_minigame.catcher.move_frames = 10;
    source->presentation.point_minigame.catcher.secondary_x = 222;
    source->presentation.point_minigame.catcher.explosion_frame = 5;
    source->presentation.point_minigame.catcher.source_draw_chunk = 14;
    source->presentation.point_minigame.catcher.draw_objects[3].active = 1;
    source->presentation.point_minigame.catcher.draw_objects[3].y = 372;
    round_trip_case(directory, "catcher-frame", source, 2, &map);
    assert(memcmp(&loaded->presentation.point_minigame,
        &source->presentation.point_minigame,sizeof(source->presentation.point_minigame)) == 0);
    memset(&source->presentation.point_minigame,0,sizeof(source->presentation.point_minigame));

    source->flow.turn_state = GAME_TURN_STATE_MOVE;
    source->flow.turn_flow_active = 1;
    source->flow.route_move.node = 1;
    source->flow.route_move.next_node = 0;
    source->flow.route_move.steps_remaining = 1;
    source->presentation.actors[0].move_active = 1;
    source->presentation.actors[0].move_from_node = 1;
    source->presentation.actors[0].move_to_node = 0;
    source->presentation.actors[0].move_progress = 7;
    source->presentation.actors[0].move_distance = 16;
    round_trip_case(directory, "movement", source, 2, &map);

    source->state.elapsed_days = 6;
    source->state.full_round_count = 6;
    source->presentation.auto_save_pending = 1;
    source->flow.turn_state = GAME_TURN_STATE_IDLE;
    source->flow.turn_flow_active = 0;
    source->presentation.actors[0].move_active = 0;
    round_trip_case(directory, "day-boundary", source, 2, &map);

    source->state.players[1].play_state = 0;
    source->state.current_player = 0;
    round_trip_case(directory, "elimination", source, 2, &map);

    /* Map identity is part of the durable state needed by the SDL adapter
     * when it commits a cross-map load and rebuilds the scene resources. */
    source->state.stage = 1;
    source->state.map = 3;
    source->time_machine_state.stage = 1;
    source->time_machine_state.map = 3;
    round_trip_case(directory, "cross-map-identity", source, 2, &map);

    source->state.players[0].monthly_loss = 123456;
    source->state.players[0].monthly_income = 78901;
    source->state.players[0].total_confinement_days = 17;
    source->time_machine_state.players[0].monthly_income = 901;
    round_trip_case(directory, "monthly-statistics", source, 2, &map);
    source->presentation.bank_settlement.eye_state = 64;
    source->presentation.bank_settlement.mouth_chunk = 23 | (22 << 8);
    round_trip_case(directory, "monthly-banker-state", source, 2, &map);
    /* Formats 4..11 called the last two listing bytes reserved and normally
     * wrote zero.  The current loader reconstructs the original type/level
     * snapshot semantics from the listed property. */
    source->state.land_type[0] = 3;
    source->state.land_level[0] = 2;
    source->state.market_listings[0][0].type = GAME_MARKET_LISTING_ASSET;
    source->state.market_listings[0][0].item_id = 0x7d1;
    source->state.market_listings[0][0].asset_type_snapshot = 0;
    source->state.market_listings[0][0].asset_level_snapshot = 0;
    assert(snprintf(damaged_source,sizeof(damaged_source),"%s/format-5.R4S",directory)>0);
    write_v5_fixture(damaged_source,source,2);
    assert(rich4_game_save_read(damaged_source,2,loaded));
    assert(loaded->state.players[0].monthly_loss==123456);
    assert(loaded->time_machine_state.players[0].monthly_income==901);
    assert(loaded->presentation.bank_settlement.eye_state==0);
    assert(loaded->presentation.bank_settlement.mouth_chunk==0);
    assert(loaded->state.market_listings[0][0].asset_type_snapshot==3);
    assert(loaded->state.market_listings[0][0].asset_level_snapshot==2);
    assert(rich4_app_map_access_mask(loaded)==0);
    remove(damaged_source);

    /* Real map 0 has 104 route nodes. The flow field stores that map count,
     * not the much smaller per-turn event queue capacity. */
    memset(large_map_routes, 0, sizeof(large_map_routes));
    large_map = map;
    large_map.routes = large_map_routes;
    large_map.route_count = 104;
    source->state.tool_route_node_count = 104;
    source->flow.route_count = 104;
    source->time_machine_state.tool_route_node_count = 104;
    source->time_machine_flow.route_count = 104;
    round_trip_case(directory, "real-map-route-count", source, 2,
                    &large_map);

    assert(snprintf(damaged_source, sizeof(damaged_source),
                    "%s/damaged-source.R4S", directory) > 0);
    /* Transient original number-wheel state cannot be omitted mid-payment. */
    source->presentation.god_acquisition.money_pending = 1;
    assert(!rich4_game_save_write(damaged_source, 2, source));
    source->presentation.god_acquisition.money_pending = 0;
    source->presentation.god_acquisition.money.active = 1;
    assert(!rich4_game_save_write(damaged_source, 2, source));
    source->presentation.god_acquisition.money.active = 0;
    assert(rich4_game_save_write(damaged_source, 2, source));
    test_bad_files(directory, damaged_source);
    remove(damaged_source);

    assert(select_card_and_tool(&card_id, &tool_id));
    assert(find_market_replay_seed(card_id, tool_id, &market_seed));
    test_ai_market_roundtrip(directory, card_id, tool_id, market_seed);
    printf("save/load round-trip and AI market replay: ok (seed=%u)\n",
           market_seed);
    return 0;
}
