#include "app/game_save.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <io.h>
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "app/audio_event.h"
#include "core/game_constants.h"
#include "core/game_domain.h"
#include "core/route_event_queue.h"
#include "core/periodic_finance.h"

static int save_game_state_matches_map(const GameState *state,
                                       const MapData *map)
{
    size_t expected_land_count;
    size_t expected_facility_count;
    size_t expected_company_count;

    if (state == NULL || map == NULL || map->routes == NULL ||
        map->route_count <= 1 ||
        map->route_count > GAME_MAX_MAP_ROUTE_NODES ||
        (map->land_count > 0 && map->lands == NULL) ||
        (map->facility_count > 0 && map->facilities == NULL) ||
        (map->auxiliary_count > 0 && map->auxiliary == NULL))
        return 0;

    expected_land_count = map->land_count > 0 ? map->land_count - 1 : 0;
    expected_facility_count = map->facility_count > 0
        ? map->facility_count - 1 : 0;
    expected_company_count = map->auxiliary_count > 0
        ? map->auxiliary_count - 1 : 0;
    if (expected_land_count > GAME_MAX_MAP_PROPERTIES ||
        expected_facility_count > GAME_MAX_MAP_PROPERTIES ||
        expected_company_count > GAME_MAX_MAP_PROPERTIES)
        return 0;

    return state->tool_route_node_count == (int)map->route_count &&
           state->land_count == (int)expected_land_count &&
           state->facility_count == (int)expected_facility_count &&
           state->company_count == (int)expected_company_count;
}

enum {
    GAME_SAVE_FORMAT_VERSION = 9,
    GAME_SAVE_HEADER_SIZE = 20,
    GAME_SAVE_MAX_FILE_SIZE = 64 * 1024 * 1024
};

typedef enum SaveCodecKind {
    SAVE_CODEC_I8,
    SAVE_CODEC_U8,
    SAVE_CODEC_I16,
    SAVE_CODEC_U16,
    SAVE_CODEC_I32,
    SAVE_CODEC_U32,
    SAVE_CODEC_I64,
    SAVE_CODEC_U64,
    SAVE_CODEC_F32
} SaveCodecKind;

typedef struct SaveCodec {
    uint8_t *data;
    size_t length;
    size_t capacity;
    size_t position;
    int reading;
    int failed;
    uint32_t version;
} SaveCodec;

static int save_codec_reserve(SaveCodec *codec, size_t extra)
{
    size_t needed;
    size_t capacity;
    uint8_t *data;
    if (codec == NULL || codec->failed || codec->reading ||
        extra > (size_t)GAME_SAVE_MAX_FILE_SIZE ||
        codec->length > (size_t)GAME_SAVE_MAX_FILE_SIZE - extra) {
        if (codec != NULL) codec->failed = 1;
        return 0;
    }
    needed = codec->length + extra;
    if (needed <= codec->capacity) return 1;
    capacity = codec->capacity == 0 ? 4096 : codec->capacity;
    while (capacity < needed) {
        if (capacity > (size_t)GAME_SAVE_MAX_FILE_SIZE / 2) {
            capacity = (size_t)GAME_SAVE_MAX_FILE_SIZE;
            break;
        }
        capacity *= 2;
    }
    data = (uint8_t *)realloc(codec->data, capacity);
    if (data == NULL) {
        codec->failed = 1;
        return 0;
    }
    codec->data = data;
    codec->capacity = capacity;
    return 1;
}

/* The on-disk schema is fixed-width little endian, not native structs. */
static int save_little_endian(void)
{
    const uint16_t one = 1;
    return *(const uint8_t *)&one == 1;
}

static void codec_scalar(SaveCodec *codec, void *value, SaveCodecKind kind)
{
    uint8_t bytes[8];
    size_t width;
    size_t i;
    if (codec == NULL || value == NULL || codec->failed) return;
    switch (kind) {
    case SAVE_CODEC_I8:
    case SAVE_CODEC_U8: width = 1; break;
    case SAVE_CODEC_I16:
    case SAVE_CODEC_U16: width = 2; break;
    case SAVE_CODEC_I32:
    case SAVE_CODEC_U32:
    case SAVE_CODEC_F32: width = 4; break;
    case SAVE_CODEC_I64:
    case SAVE_CODEC_U64: width = 8; break;
    default: codec->failed = 1; return;
    }
    if (save_little_endian()) {
        if (codec->reading) {
            if (codec->position > codec->length ||
                width > codec->length - codec->position) {
                codec->failed = 1;
                return;
            }
            memcpy(value, codec->data + codec->position, width);
            codec->position += width;
        } else if (save_codec_reserve(codec, width)) {
            memcpy(codec->data + codec->length, value, width);
            codec->length += width;
        }
        return;
    }
    if (codec->reading) {
        if (codec->position > codec->length ||
            codec->length - codec->position < width) {
            codec->failed = 1;
            return;
        }
        if (width == 1) {
            bytes[0] = codec->data[codec->position];
        } else {
            for (i = 0; i < width; i++)
                bytes[i] = codec->data[codec->position + i];
        }
        codec->position += width;
        if (width == 2) {
            uint16_t bits = (uint16_t)bytes[0] |
                            (uint16_t)((uint16_t)bytes[1] << 8);
            memcpy(value, &bits, sizeof(bits));
        } else if (width == 4) {
            uint32_t bits = (uint32_t)bytes[0] |
                ((uint32_t)bytes[1] << 8) |
                ((uint32_t)bytes[2] << 16) |
                ((uint32_t)bytes[3] << 24);
            memcpy(value, &bits, sizeof(bits));
        } else if (width == 8) {
            uint64_t bits = 0;
            for (i = 0; i < 8; i++) bits |= (uint64_t)bytes[i] << (i * 8);
            memcpy(value, &bits, sizeof(bits));
        } else {
            memcpy(value, bytes, width);
        }
        return;
    }
    if (!save_codec_reserve(codec, width)) return;
    if (width == 2) {
        uint16_t bits;
        memcpy(&bits, value, sizeof(bits));
        bytes[0] = (uint8_t)bits;
        bytes[1] = (uint8_t)(bits >> 8);
    } else if (width == 4) {
        uint32_t bits;
        memcpy(&bits, value, sizeof(bits));
        for (i = 0; i < 4; i++) bytes[i] = (uint8_t)(bits >> (i * 8));
    } else if (width == 8) {
        uint64_t bits;
        memcpy(&bits, value, sizeof(bits));
        for (i = 0; i < 8; i++) bytes[i] = (uint8_t)(bits >> (i * 8));
    } else {
        memcpy(bytes, value, width);
    }
    memcpy(codec->data + codec->length, bytes, width);
    codec->length += width;
}

static void codec_array(SaveCodec *codec, void *value, size_t count,
                        size_t width, SaveCodecKind kind)
{
    size_t size, i;
    if (codec->failed) return;
    if (width == 0 || count > GAME_SAVE_MAX_FILE_SIZE / width) {
        codec->failed = 1;
        return;
    }
    size = count * width;
    if (!save_little_endian()) {
        for (i = 0; i < count; i++)
            codec_scalar(codec, (uint8_t *)value + i * width, kind);
    } else if (codec->reading) {
        if (codec->position > codec->length || size > codec->length - codec->position) {
            codec->failed = 1;
            return;
        }
        memcpy(value, codec->data + codec->position, size);
        codec->position += size;
    } else if (save_codec_reserve(codec, size)) {
        memcpy(codec->data + codec->length, value, size);
        codec->length += size;
    }
}

#include "app/game_save_codec.inc"
#include "app/game_save_codec_v4.inc"
#include "app/game_save_codec_v5.inc"

/* Same IEEE CRC32 as the bitwise codec; immutable table is thread-safe. */
static uint32_t save_crc32(const uint8_t *bytes, size_t length)
{
    static const uint32_t table[256] = {
        0x00000000u, 0x77073096u, 0xee0e612cu, 0x990951bau, 0x076dc419u, 0x706af48fu, 0xe963a535u, 0x9e6495a3u,
        0x0edb8832u, 0x79dcb8a4u, 0xe0d5e91eu, 0x97d2d988u, 0x09b64c2bu, 0x7eb17cbdu, 0xe7b82d07u, 0x90bf1d91u,
        0x1db71064u, 0x6ab020f2u, 0xf3b97148u, 0x84be41deu, 0x1adad47du, 0x6ddde4ebu, 0xf4d4b551u, 0x83d385c7u,
        0x136c9856u, 0x646ba8c0u, 0xfd62f97au, 0x8a65c9ecu, 0x14015c4fu, 0x63066cd9u, 0xfa0f3d63u, 0x8d080df5u,
        0x3b6e20c8u, 0x4c69105eu, 0xd56041e4u, 0xa2677172u, 0x3c03e4d1u, 0x4b04d447u, 0xd20d85fdu, 0xa50ab56bu,
        0x35b5a8fau, 0x42b2986cu, 0xdbbbc9d6u, 0xacbcf940u, 0x32d86ce3u, 0x45df5c75u, 0xdcd60dcfu, 0xabd13d59u,
        0x26d930acu, 0x51de003au, 0xc8d75180u, 0xbfd06116u, 0x21b4f4b5u, 0x56b3c423u, 0xcfba9599u, 0xb8bda50fu,
        0x2802b89eu, 0x5f058808u, 0xc60cd9b2u, 0xb10be924u, 0x2f6f7c87u, 0x58684c11u, 0xc1611dabu, 0xb6662d3du,
        0x76dc4190u, 0x01db7106u, 0x98d220bcu, 0xefd5102au, 0x71b18589u, 0x06b6b51fu, 0x9fbfe4a5u, 0xe8b8d433u,
        0x7807c9a2u, 0x0f00f934u, 0x9609a88eu, 0xe10e9818u, 0x7f6a0dbbu, 0x086d3d2du, 0x91646c97u, 0xe6635c01u,
        0x6b6b51f4u, 0x1c6c6162u, 0x856530d8u, 0xf262004eu, 0x6c0695edu, 0x1b01a57bu, 0x8208f4c1u, 0xf50fc457u,
        0x65b0d9c6u, 0x12b7e950u, 0x8bbeb8eau, 0xfcb9887cu, 0x62dd1ddfu, 0x15da2d49u, 0x8cd37cf3u, 0xfbd44c65u,
        0x4db26158u, 0x3ab551ceu, 0xa3bc0074u, 0xd4bb30e2u, 0x4adfa541u, 0x3dd895d7u, 0xa4d1c46du, 0xd3d6f4fbu,
        0x4369e96au, 0x346ed9fcu, 0xad678846u, 0xda60b8d0u, 0x44042d73u, 0x33031de5u, 0xaa0a4c5fu, 0xdd0d7cc9u,
        0x5005713cu, 0x270241aau, 0xbe0b1010u, 0xc90c2086u, 0x5768b525u, 0x206f85b3u, 0xb966d409u, 0xce61e49fu,
        0x5edef90eu, 0x29d9c998u, 0xb0d09822u, 0xc7d7a8b4u, 0x59b33d17u, 0x2eb40d81u, 0xb7bd5c3bu, 0xc0ba6cadu,
        0xedb88320u, 0x9abfb3b6u, 0x03b6e20cu, 0x74b1d29au, 0xead54739u, 0x9dd277afu, 0x04db2615u, 0x73dc1683u,
        0xe3630b12u, 0x94643b84u, 0x0d6d6a3eu, 0x7a6a5aa8u, 0xe40ecf0bu, 0x9309ff9du, 0x0a00ae27u, 0x7d079eb1u,
        0xf00f9344u, 0x8708a3d2u, 0x1e01f268u, 0x6906c2feu, 0xf762575du, 0x806567cbu, 0x196c3671u, 0x6e6b06e7u,
        0xfed41b76u, 0x89d32be0u, 0x10da7a5au, 0x67dd4accu, 0xf9b9df6fu, 0x8ebeeff9u, 0x17b7be43u, 0x60b08ed5u,
        0xd6d6a3e8u, 0xa1d1937eu, 0x38d8c2c4u, 0x4fdff252u, 0xd1bb67f1u, 0xa6bc5767u, 0x3fb506ddu, 0x48b2364bu,
        0xd80d2bdau, 0xaf0a1b4cu, 0x36034af6u, 0x41047a60u, 0xdf60efc3u, 0xa867df55u, 0x316e8eefu, 0x4669be79u,
        0xcb61b38cu, 0xbc66831au, 0x256fd2a0u, 0x5268e236u, 0xcc0c7795u, 0xbb0b4703u, 0x220216b9u, 0x5505262fu,
        0xc5ba3bbeu, 0xb2bd0b28u, 0x2bb45a92u, 0x5cb36a04u, 0xc2d7ffa7u, 0xb5d0cf31u, 0x2cd99e8bu, 0x5bdeae1du,
        0x9b64c2b0u, 0xec63f226u, 0x756aa39cu, 0x026d930au, 0x9c0906a9u, 0xeb0e363fu, 0x72076785u, 0x05005713u,
        0x95bf4a82u, 0xe2b87a14u, 0x7bb12baeu, 0x0cb61b38u, 0x92d28e9bu, 0xe5d5be0du, 0x7cdcefb7u, 0x0bdbdf21u,
        0x86d3d2d4u, 0xf1d4e242u, 0x68ddb3f8u, 0x1fda836eu, 0x81be16cdu, 0xf6b9265bu, 0x6fb077e1u, 0x18b74777u,
        0x88085ae6u, 0xff0f6a70u, 0x66063bcau, 0x11010b5cu, 0x8f659effu, 0xf862ae69u, 0x616bffd3u, 0x166ccf45u,
        0xa00ae278u, 0xd70dd2eeu, 0x4e048354u, 0x3903b3c2u, 0xa7672661u, 0xd06016f7u, 0x4969474du, 0x3e6e77dbu,
        0xaed16a4au, 0xd9d65adcu, 0x40df0b66u, 0x37d83bf0u, 0xa9bcae53u, 0xdebb9ec5u, 0x47b2cf7fu, 0x30b5ffe9u,
        0xbdbdf21cu, 0xcabac28au, 0x53b39330u, 0x24b4a3a6u, 0xbad03605u, 0xcdd70693u, 0x54de5729u, 0x23d967bfu,
        0xb3667a2eu, 0xc4614ab8u, 0x5d681b02u, 0x2a6f2b94u, 0xb40bbe37u, 0xc30c8ea1u, 0x5a05df1bu, 0x2d02ef8du,
    };
    uint32_t crc = 0xffffffffu;
    size_t i;
    for (i = 0; i < length; i++)
        crc = (crc >> 8) ^ table[(crc ^ bytes[i]) & 255u];
    return ~crc;
}

/* Temporary diagnostic seam, independent of platform clocks and SDL. */
static Rich4SaveDebugObserver save_observer;
void rich4_game_save_set_debug_observer(Rich4SaveDebugObserver observer)
{
    save_observer = observer;
}
static void save_observe(const char *phase, int begin, size_t bytes)
{
    if (save_observer != NULL) save_observer(phase, begin, bytes);
}

static void save_put_u32(uint8_t *out, uint32_t value)
{
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8);
    out[2] = (uint8_t)(value >> 16);
    out[3] = (uint8_t)(value >> 24);
}

static uint32_t save_get_u32(const uint8_t *in)
{
    return (uint32_t)in[0] | ((uint32_t)in[1] << 8) |
           ((uint32_t)in[2] << 16) | ((uint32_t)in[3] << 24);
}

static int save_slot_valid(int slot)
{
    return slot >= GAME_SAVE_AUTO_SLOT && slot < GAME_SAVE_SLOT_COUNT;
}

int rich4_game_save_slot_path(char *out, size_t capacity,
                              const char *directory, int slot)
{
    size_t length;
    int written;
    if (out == NULL || capacity == 0 || directory == NULL ||
        directory[0] == '\0' || !save_slot_valid(slot))
        return 0;
    length = strlen(directory);
    written = snprintf(out, capacity, "%s%sSAVE%d.R4S", directory,
                       directory[length - 1] == '/' ||
                       directory[length - 1] == '\\' ? "" : "/", slot);
    return written >= 0 && (size_t)written < capacity;
}

int rich4_game_save_ensure_directory(const char *directory)
{
    if (directory == NULL || directory[0] == '\0') return 0;
#if defined(_WIN32)
    if (CreateDirectoryA(directory, NULL)) return 1;
    {
        DWORD attributes = GetFileAttributesA(directory);
        return attributes != INVALID_FILE_ATTRIBUTES &&
               (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }
#else
    if (mkdir(directory, 0700) == 0) return 1;
    {
        struct stat info;
        return stat(directory, &info) == 0 && S_ISDIR(info.st_mode);
    }
#endif
}

static int save_game_state_valid(const GameState *state)
{
    int i;
    if (state == NULL || !state->active || state->player_count < 1 ||
        state->player_count > GAME_MAX_PLAYERS || state->current_player < 0 ||
        state->current_player >= state->player_count ||
        state->stage < 0 || state->stage >= GAME_STAGE_COUNT ||
        state->map < 0 || state->map >= GAME_MAPS_PER_STAGE ||
        state->tool_route_node_count < 0 ||
        state->tool_route_node_count > GAME_MAX_MAP_ROUTE_NODES ||
        state->land_count < 0 || state->land_count > GAME_MAX_MAP_PROPERTIES ||
        state->facility_count < 0 ||
        state->facility_count > GAME_MAX_MAP_PROPERTIES ||
        state->company_count < 0 ||
        state->company_count > GAME_MAX_MAP_PROPERTIES)
        return 0;
    for (i = 0; i < state->player_count; i++) {
        int node = state->players[i].node;
        if (state->tool_route_node_count > 0 &&
            ((node < 0 &&
              ((state->players[i].play_state &
                GAME_PLAYER_PLAY_STATE_MASK) != 0 || node != -1)) ||
             node >= state->tool_route_node_count))
            return 0;
    }
    for (i = 0; i < state->land_count; i++)
        if (state->land_owner[i] < -1 ||
            state->land_owner[i] >= state->player_count)
            return 0;
    for (i = 0; i < state->facility_count; i++)
        if (state->facility_owner[i] < -1 ||
            state->facility_owner[i] >= state->player_count)
            return 0;
    for (i = 0; i < state->company_count; i++)
        if (state->company_owner[i] < -1 ||
            state->company_owner[i] >= state->player_count)
            return 0;
    return 1;
}

int rich4_game_save_session_matches_map(const GameSession *session,
                                        const MapData *map)
{
    int route_count;
    int i;
    if (session == NULL ||
        !save_game_state_matches_map(&session->state, map) ||
        (session->time_machine_valid &&
         !save_game_state_matches_map(&session->time_machine_state, map)))
        return 0;
    route_count = (int)map->route_count;

    for (i = 0; i < GAME_MAP_DYNAMIC_OBJECT_COUNT; i++) {
        const GameMapDynamicObjectState *object =
            &session->presentation.dynamic_objects[i];
        if (object->node >= map->route_count && object->node != 0)
            return 0;
        if (session->time_machine_valid) {
            object = &session->time_machine_presentation.dynamic_objects[i];
            if (object->node >= map->route_count && object->node != 0)
                return 0;
        }
    }
    for (i = 0; i < GAME_TOOL_PLACED_OBJECT_COUNT; i++) {
        const struct GameToolPlacedObjectState *object =
            &session->state.tool_placed_objects[i];
        if (object->kind != 0 &&
            (object->node <= 0 || object->node >= route_count))
            return 0;
        if (session->time_machine_valid) {
            object = &session->time_machine_state.tool_placed_objects[i];
            if (object->kind != 0 &&
                (object->node <= 0 || object->node >= route_count))
                return 0;
        }
    }
    return 1;
}

static int save_session_valid(const GameSession *session)
{
    int i;
    const GameState *state;
    const GameFlowState *flow;
    const GamePresentationState *presentation;
    if (session == NULL || !save_game_state_valid(&session->state) ||
        session->time_machine_valid < 0 || session->time_machine_valid > 1)
        return 0;
    state = &session->state;
    flow = &session->flow;
    presentation = &session->presentation;
    /* These original blocking modals do not admit a save operation. */
    if (presentation->special_place_notice.queue.count > 0 ||
        (session->time_machine_valid &&
         session->time_machine_presentation.special_place_notice.queue.count > 0))
        return 0;
    if (session->pending_turn_events.count > 0 ||
        presentation->periodic_finance.active || presentation->bank_settlement.active ||
        (session->time_machine_valid &&
         (session->time_machine_presentation.periodic_finance.active ||
          session->time_machine_presentation.bank_settlement.active))) return 0;
    if (presentation->hotel_entry.pending || presentation->hotel_entry.active ||
        (presentation->confinement_exit.active &&
         presentation->confinement_exit.kind == GAME_CONFINEMENT_HOTEL) ||
        (session->time_machine_valid &&
         (session->time_machine_presentation.hotel_entry.pending ||
          session->time_machine_presentation.hotel_entry.active ||
          (session->time_machine_presentation.confinement_exit.active &&
           session->time_machine_presentation.confinement_exit.kind == GAME_CONFINEMENT_HOTEL)))) return 0;
    if (presentation->god_detachment.active || presentation->god_detachment.speech_pending ||
        (session->time_machine_valid && (session->time_machine_presentation.god_detachment.active ||
            session->time_machine_presentation.god_detachment.speech_pending))) return 0;
    if (presentation->property.insurance_active ||
        (presentation->company_issue.active && presentation->company_issue.phase == 1) ||
        (session->time_machine_valid &&
         (session->time_machine_presentation.property.insurance_active ||
          (session->time_machine_presentation.company_issue.active &&
           session->time_machine_presentation.company_issue.phase == 1)))) return 0;
    if (presentation->property.god_type != 0 ||
        (session->time_machine_valid &&
         session->time_machine_presentation.property.god_type != 0)) return 0;
    if (presentation->engineering_attack.active ||
        presentation->engineering_attack.pending_return ||
        (session->time_machine_valid &&
         (session->time_machine_presentation.engineering_attack.active ||
          session->time_machine_presentation.engineering_attack.pending_return)))
        return 0;
    if (presentation->god_acquisition.death_pending ||
        presentation->god_acquisition.arrival_speech_phase ||
        (session->time_machine_valid &&
         (session->time_machine_presentation.god_acquisition.death_pending ||
          session->time_machine_presentation.god_acquisition.arrival_speech_phase)) ||
        presentation->god_acquisition.money_pending ||
        presentation->god_acquisition.waiting_for_detach ||
        presentation->god_acquisition.money.active ||
        (session->time_machine_valid &&
         (session->time_machine_presentation.god_acquisition.money_pending ||
          session->time_machine_presentation.god_acquisition.waiting_for_detach ||
          session->time_machine_presentation.god_acquisition.money.active)))
        return 0;
    if (flow->turn_state < GAME_TURN_STATE_IDLE ||
        flow->turn_state > GAME_TURN_STATE_ROUTE_EVENTS ||
        flow->route_count < 0 || flow->route_count > GAME_MAX_MAP_ROUTE_NODES ||
        flow->route_facts.count < 0 ||
        flow->route_facts.count > ROUTE_EVENT_COLLECTION_CAPACITY ||
        flow->route_events.queue.count < 0 ||
        flow->route_events.queue.count > ROUTE_EVENT_QUEUE_CAPACITY ||
        flow->route_events.queue.head < 0 ||
        flow->route_events.queue.head >= ROUTE_EVENT_QUEUE_CAPACITY ||
        flow->route_events.queue.tail < 0 ||
        flow->route_events.queue.tail >= ROUTE_EVENT_QUEUE_CAPACITY ||
        presentation->mode < RICH4_PRESENTATION_NONE ||
        presentation->mode > RICH4_PRESENTATION_GOD)
        return 0;
    for (i = 0; i < GAME_MAX_PLAYERS; i++) {
        int node = session->presentation.actors[i].move_to_node;
        if (session->presentation.actors[i].move_active &&
            (node < 0 || node >= state->tool_route_node_count))
            return 0;
    }
    if (session->time_machine_valid &&
        !save_game_state_valid(&session->time_machine_state))
        return 0;
    if (!rich4_presentation_speech_saveable(presentation) ||
        (session->time_machine_valid &&
         !rich4_presentation_speech_saveable(
             &session->time_machine_presentation)))
        return 0;
    return 1;
}

static int save_replace_file(const char *temporary, const char *destination)
{
#if defined(_WIN32)
    return MoveFileExA(temporary, destination,
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    return rename(temporary, destination) == 0;
#endif
}

static int save_flush_file(FILE *file)
{
    if (fflush(file) != 0) return 0;
#if defined(_WIN32)
    return _commit(_fileno(file)) == 0;
#else
    return fsync(fileno(file)) == 0;
#endif
}

int rich4_game_save_write(const char *path, int slot,
                          const GameSession *session)
{
    static const uint8_t magic[4] = {'R', '4', 'S', 'V'};
    SaveCodec codec;
    uint8_t header[GAME_SAVE_HEADER_SIZE];
    char temporary[RICH4_GAME_SAVE_PATH_CAPACITY + 8];
    FILE *file;
    size_t path_length;
    int ok;
    if (path == NULL || path[0] == '\0' || !save_slot_valid(slot) ||
        !save_session_valid(session) || sizeof(float) != 4)
        return 0;
    memset(&codec, 0, sizeof(codec));
    codec.version = GAME_SAVE_FORMAT_VERSION;
    save_observe("encode", 1, 0);
    codec_GameSession(&codec, (GameSession *)session);
    save_observe("encode", 0, codec.length);
    if (codec.failed || codec.length > UINT32_MAX) {
        free(codec.data);
        return 0;
    }
    memset(header, 0, sizeof(header));
    memcpy(header, magic, sizeof(magic));
    save_put_u32(header + 4, GAME_SAVE_FORMAT_VERSION);
    save_put_u32(header + 8, (uint32_t)slot);
    save_put_u32(header + 12, (uint32_t)codec.length);
    save_observe("crc", 1, codec.length);
    save_put_u32(header + 16, save_crc32(codec.data, codec.length));
    save_observe("crc", 0, codec.length);
    path_length = strlen(path);
    if (path_length + 5 >= sizeof(temporary)) {
        free(codec.data);
        return 0;
    }
    memcpy(temporary, path, path_length);
    memcpy(temporary + path_length, ".tmp", 5);
    save_observe("write", 1, codec.length);
    file = fopen(temporary, "wb");
    if (file == NULL) {
        save_observe("write", 0, codec.length);
        free(codec.data);
        return 0;
    }
    ok = fwrite(header, 1, sizeof(header), file) == sizeof(header) &&
         fwrite(codec.data, 1, codec.length, file) == codec.length;
    save_observe("write", 0, codec.length);
    save_observe("flush", 1, codec.length);
    if (ok) ok = save_flush_file(file);
    if (fclose(file) != 0) ok = 0;
    save_observe("flush", 0, codec.length);
    save_observe("replace", 1, codec.length);
    if (ok) ok = save_replace_file(temporary, path);
    save_observe("replace", 0, codec.length);
    if (!ok) {
        remove(temporary);
    }
    free(codec.data);
    return ok;
}

int rich4_game_save_resume(GameSession *session, const GameSession *loaded,
                           uint32_t now)
{
    uint32_t clock_offset;
    if (session == NULL || loaded == NULL) return 0;
    clock_offset = session->rng_clock_offset;
    *session = *loaded;
    session->rng_clock_offset = clock_offset;
    session->state.ai_rng_state = now + clock_offset;
    return rich4_presentation_rebind_speech(&session->presentation) &&
        (!session->time_machine_valid ||
         rich4_presentation_rebind_speech(&session->time_machine_presentation));
}

int rich4_game_save_read(const char *path, int slot,
                         GameSession *session)
{
    static const uint8_t magic[4] = {'R', '4', 'S', 'V'};
    FILE *file;
    long file_size;
    uint8_t *bytes;
    uint32_t payload_size;
    GameSession *decoded;
    SaveCodec codec;
    int ok;
    if (path == NULL || path[0] == '\0' || session == NULL ||
        !save_slot_valid(slot) || sizeof(float) != 4)
        return 0;
    file = fopen(path, "rb");
    if (file == NULL) return 0;
    if (fseek(file, 0, SEEK_END) != 0 || (file_size = ftell(file)) < 0 ||
        file_size < GAME_SAVE_HEADER_SIZE ||
        file_size > GAME_SAVE_MAX_FILE_SIZE || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return 0;
    }
    bytes = (uint8_t *)malloc((size_t)file_size);
    if (bytes == NULL) {
        fclose(file);
        return 0;
    }
    ok = fread(bytes, 1, (size_t)file_size, file) == (size_t)file_size;
    if (fclose(file) != 0) ok = 0;
    if (!ok || memcmp(bytes, magic, sizeof(magic)) != 0 ||
        (save_get_u32(bytes + 4) != GAME_SAVE_FORMAT_VERSION &&
         save_get_u32(bytes + 4) != 8 &&
         save_get_u32(bytes + 4) != 7 &&
         save_get_u32(bytes + 4) != 6 &&
         save_get_u32(bytes + 4) != 5 &&
         save_get_u32(bytes + 4) != 4) ||
        save_get_u32(bytes + 8) != (uint32_t)slot) {
        free(bytes);
        return 0;
    }
    payload_size = save_get_u32(bytes + 12);
    if ((uint64_t)payload_size + GAME_SAVE_HEADER_SIZE !=
            (uint64_t)file_size ||
        save_crc32(bytes + GAME_SAVE_HEADER_SIZE, payload_size) !=
            save_get_u32(bytes + 16)) {
        free(bytes);
        return 0;
    }
    decoded = (GameSession *)calloc(1, sizeof(*decoded));
    if (decoded == NULL) {
        free(bytes);
        return 0;
    }
    memset(&codec, 0, sizeof(codec));
    codec.data = bytes + GAME_SAVE_HEADER_SIZE;
    codec.version = save_get_u32(bytes + 4);
    codec.length = payload_size;
    codec.reading = 1;
    if (save_get_u32(bytes + 4) == 4) {
        codec_v4_GameSession(&codec, decoded);
        /* Format 4 allowed an autosave inside the already-credited monthly
         * report. It has no recoverable stage/voice/animation continuation.
         * Complete that report's cleanup without paying the interest again. */
        if (decoded->presentation.bank_settlement.active) {
            rich4_monthly_clear_statistics(&decoded->state,
                decoded->presentation.bank_settlement.player_mask);
            decoded->presentation.bank_settlement.active = 0;
        }
        if (decoded->time_machine_valid &&
            decoded->time_machine_presentation.bank_settlement.active) {
            rich4_monthly_clear_statistics(&decoded->time_machine_state,
                decoded->time_machine_presentation.bank_settlement.player_mask);
            decoded->time_machine_presentation.bank_settlement.active = 0;
        }
    } else if (save_get_u32(bytes + 4) == 5)
        codec_v5_GameSession(&codec, decoded);
    else codec_GameSession(&codec, decoded);
    if (codec.version < 9) {
        GamePresentationState *presentations[2] = {
            &decoded->presentation, &decoded->time_machine_presentation
        };
        GameState *states[2] = {&decoded->state, &decoded->time_machine_state};
        static const unsigned char frames[12] = {10,10,10,14,10,13,10,11,13,11,10,11};
        int p, i;
        for (p = 0; p < (decoded->time_machine_valid ? 2 : 1); p++) {
            struct PointMinigameState *g = &presentations[p]->point_minigame;
            int character;
            if (!g->active || g->action != 8 || g->automatic) continue;
            character = g->player >= 0 && g->player < states[p]->player_count
                ? states[p]->players[g->player].character : 0;
            g->catcher.move_frames = frames[character >= 0 && character < 12 ? character : 0];
            g->catcher.mouse_warp = -1;
            g->catcher.scene_initialized = 1;
            g->catcher.stage = g->ticks_remaining <= 0 || g->catcher_bomb_active ? 1 : 0;
            g->catcher.drop_frame = g->animation_delay;
            g->catcher_source_direction = g->catcher_source_direction > 0 ? 0 : 4;
            g->catcher.source_draw_x = g->catcher_source_x;
            g->catcher.source_draw_chunk = g->catcher_source_direction == 0 ? 0 : 12;
            g->catcher.secondary_x = g->catcher_source_x;
            g->catcher.secondary_draw_frame = g->catcher_secondary_active
                ? g->catcher_secondary_frame : -1;
            for (i = 0; i < GAME_POINT_MINIGAME_OBJECT_COUNT; i++) {
                g->catcher.draw_objects[i] = g->object[i];
                if (g->object[i].active && g->object[i].type == 4) g->catcher.bombs++;
            }
        }
    }
    if (codec.version < 8) {
        GamePresentationState *presentations[2] = {
            &decoded->presentation, &decoded->time_machine_presentation
        };
        GameState *states[2] = {&decoded->state, &decoded->time_machine_state};
        int p, slot;
        for (p = 0; p < (decoded->time_machine_valid ? 2 : 1); p++) {
            for (slot = 0; slot < GAME_EVENT_QUEUE_CAPACITY; slot++) {
                GameToolAnimationRequest *request = &presentations[p]->tool_animation[slot];
                int actor = request->after_sync_player;
                if (codec.version < 7 && request->active && request->resource == 525 &&
                    actor >= 0 && actor < states[p]->player_count)
                    request->explosion_traffic_method = states[p]->players[actor].traffic_method;
                if (request->active && request->resource == 524 && actor >= 0 &&
                    actor < states[p]->player_count)
                    request->origin_play_state = states[p]->players[actor].play_state |
                        GAME_PLAYER_PLAY_STATE_FROZEN;
            }
        }
    }
    ok = !codec.failed && codec.position == codec.length &&
         rich4_presentation_rebind_speech(&decoded->presentation) &&
         (!decoded->time_machine_valid ||
          rich4_presentation_rebind_speech(
              &decoded->time_machine_presentation)) &&
         save_session_valid(decoded);
    if (ok) {
        rich4_audio_event_queue_init(&decoded->audio_events);
        rich4_game_event_queue_reset(&decoded->deferred_events);
        rich4_game_event_queue_reset(&decoded->pending_turn_events);
        rich4_game_event_queue_reset(&decoded->bankruptcy_auction_events);
        rich4_game_event_queue_reset(&decoded->bankruptcy_terminal_events);
        *session = *decoded;
        ok = rich4_presentation_rebind_speech(&session->presentation) &&
            (!session->time_machine_valid ||
             rich4_presentation_rebind_speech(
                 &session->time_machine_presentation));
    }
    free(decoded);
    free(bytes);
    return ok;
}
