#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/ai_flow.h"
#include "app/audio_event.h"
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
    session->time_machine_state = session->state;
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
    initialize_map(&map);
    initialize_session(source);
    test_failed_write_preserves_slot(directory, source);

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
    assert(snprintf(damaged_source,sizeof(damaged_source),"%s/format-5.R4S",directory)>0);
    write_v5_fixture(damaged_source,source,2);
    assert(rich4_game_save_read(damaged_source,2,loaded));
    assert(loaded->state.players[0].monthly_loss==123456);
    assert(loaded->time_machine_state.players[0].monthly_income==901);
    assert(loaded->presentation.bank_settlement.eye_state==0);
    assert(loaded->presentation.bank_settlement.mouth_chunk==0);
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
