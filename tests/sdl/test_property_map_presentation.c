#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL/SDL.h>
#undef main

#include "core/card.h"
#include "core/turn_rotation.h"
#include "platform/sdl1/render/game_map_objects.h"
#include "platform/sdl1/render/blit.h"
#include "resource/mkf.h"
#include "app/property_flow.h"
#include "platform/sdl1/runtime_trace.h"

static GamePreviewAssets assets;
static GameState state;
static GameViewModel view;
static GameViewModelInput input;
static MapLandRecord lands[33];
static MapFacilityRecord facilities[2];
static NewGameSettings settings;

static void setup(int facility)
{
    int i;
    memset(&state, 0, sizeof(state));
    memset(&input, 0, sizeof(input));
    state.active = 1;
    state.player_count = 2;
    state.price_index = 1;
    state.calendar_start_year = 2000;
    state.calendar_start_month = 1;
    state.calendar_start_day = 3;
    for (i = 0; i < 2; i++) {
        state.players[i].play_state = 1;
        state.players[i].cash = 1000;
        state.players[i].linked_player = -1;
    }
    for (i = 0; i < GAME_SPECIAL_PLAYER_COUNT; i++) state.special_player_f10[i] = 1;
    state.land_count = !facility;
    state.facility_count = facility;
    state.land_owner[0] = state.facility_owner[0] = -1;
    input.has_view_override = 1;
    input.view_world_x = 352;
    input.view_world_y = 640;
    lands[1].x = facilities[1].x = 352;
    lands[1].y = facilities[1].y = 640;
}

static uint16_t pixel(SDL_Surface *screen, int x, int y)
{
    return ((uint16_t *)((unsigned char *)screen->pixels + y * screen->pitch))[x];
}

/* Exercise command -> day settlement -> view model -> real Map.mkf[26]
 * pixels. Static map fields are deliberately stale in both directions. */
static void check_card_markers(SDL_Surface *screen)
{
    int facility, card, rotation, day, x, y;
    for (facility = 0; facility < 2; facility++) {
        for (card = 26; card <= 27; card++) {
            for (rotation = 0; rotation < 8; rotation++) {
                GameCommand command;
                GameEventQueue events;
                Rich4SprSmpChunk mask;
                int anchor_x, anchor_y;
                setup(facility);
                lands[1].parity_overlay = facilities[1].parity_overlay = 0x51;
                input.map_direction = rotation;
                state.players[0].card_inventory[0] = card + 1;
                command = rich4_game_domain_command(GAME_COMMAND_USE_CARD,
                    GAME_COMMAND_SOURCE_HUMAN, 0,
                    rich4_game_domain_ref(GAME_DOMAIN_CARD, card),
                    rich4_game_domain_ref(facility ? GAME_DOMAIN_FACILITY : GAME_DOMAIN_LAND, 0), 0);
                rich4_game_event_queue_reset(&events);
                assert(rich4_card_apply_command(&state, &command, &events) == GAME_CARD_USE_APPLIED);
                for (day = 0; day <= 5; day++) {
                    uint16_t color = card == 26 ? 0xf800 : 0x003f;
                    assert(rich4_game_view_model_build(&state, &input, &assets.map.maps[0], &view));
                    assert(rich4_spr_smp_get_chunk(assets.map.map_common_data[2],
                        assets.map.map_common_size[2], &assets.map.map_common_spr[2],
                        (size_t)(facility * 2 + (rotation & 1)), &mask) == RICH4_SPR_SMP_OK);
                    rich4_game_viewport_project(&view.viewport, 352, 640, &anchor_x, &anchor_y);
                    SDL_FillRect(screen, NULL, 0x2104);
                    draw_game_map_objects(screen, &assets, &settings, &view, NULL);
                    for (y = 0; y < 480; y++) for (x = 0; x < 640; x++) {
                        int mx = x - (anchor_x - mask.x);
                        int my = y - (anchor_y - mask.y);
                        uint16_t expected = 0x2104;
                        if (day < 5 && mx >= 0 && mx < mask.width && my >= 0 && my < mask.height &&
                            assets.map.map_common_data[2][mask.data_offset + my * mask.width + mx])
                            expected |= color;
                        assert(pixel(screen, x, y) == expected);
                    }
                    if (day < 5) {
                        TurnAdvanceResult result;
                        state.turn_rotation_index = 1;
                        rich4_game_event_queue_reset(&events);
                        assert(rich4_turn_advance_once_events(&state, &result, &events));
                        assert(state.elapsed_days == day + 1);
                    }
                }
            }
        }
    }
}

static void check_flash_footprint_and_occlusion(SDL_Surface *screen)
{
    Rich4SprSmpChunk mask;
    int rotation, actor, x, y;
    setup(0);
    lands[1].parity_overlay = 0;
    for (rotation = 0; rotation < 8; rotation++) {
        input.map_direction = rotation;
        assert(rich4_game_view_model_build(&state, &input, &assets.map.maps[0], &view));
        view.property_flash_adjust = 8;
        view.lands[0].flash_adjust = 8;
        assert(rich4_spr_smp_get_chunk(assets.map.map_common_data[2],
            assets.map.map_common_size[2], &assets.map.map_common_spr[2],
            (size_t)(rotation & 1), &mask) == RICH4_SPR_SMP_OK);
        for (actor = 0; actor < 2; actor++) {
            int cx, cy;
            rich4_game_viewport_project(&view.viewport, 352, 640, &cx, &cy);
            view.actor_count = actor;
            view.actors[0].visible = 1;
            view.actors[0].world_x = 352;
            view.actors[0].world_y = 640;
            view.actors[0].overlay_part = -1;
            SDL_FillRect(screen, NULL, 0x2104);
            draw_game_property_flash(screen, &assets, &settings, &view, NULL);
            for (y = 40; y < 480; y++) for (x = 0; x < 440; x++) {
                int mx = x - (cx - mask.x), my = y - (cy - mask.y);
                int covered = mx >= 0 && mx < mask.width && my >= 0 && my < mask.height &&
                    assets.map.map_common_data[2][mask.data_offset + my * mask.width + mx];
                uint16_t expected = 0x2104;
                if (actor && x >= cx - 7 && x < cx + 7 && y >= cy - 7 && y < cy + 7) covered = 0;
                /* LUT at 0x485D68: a=8,c=4 -> 11. */
                if (covered) expected = map_resource_pixel(screen, (uint16_t)((11 << 10) | (11 << 5) | 11));
                assert(pixel(screen, x, y) == expected);
            }
        }
    }
}

static void check_flash_building_pixels(SDL_Surface *screen, Rich4Mkf *mkf)
{
    unsigned char *data;
    size_t size;
    Rich4SprSmpChunk building, ground;
    int x, y, cx, cy, found = 0;
    assert(rich4_mkf_read_resource(mkf, 39, &data, &size, NULL) == RICH4_MKF_OK);
    /* Map 0's level-1 view bucket: 0x407E3B, Map[39]. */
    assets.map.map_object_data[1] = data;
    assets.map.map_object_size[1] = size;
    assets.map.map_object_resource_present[1] = assets.map.has_map_object_sprite[1] = 1;
    assert(rich4_spr_smp_probe(data, size, &assets.map.map_object_spr[1]));
    assert(rich4_spr_smp_get_chunk(data, size, &assets.map.map_object_spr[1],
        0, &building) == RICH4_SPR_SMP_OK);
    assert(rich4_spr_smp_get_chunk(assets.map.map_common_data[2],
        assets.map.map_common_size[2], &assets.map.map_common_spr[2],
        0, &ground) == RICH4_SPR_SMP_OK);
    setup(0);
    state.land_level[0] = 1;
    state.land_owner[0] = 1;
    assert(rich4_game_view_model_build(&state, &input, &assets.map.maps[0], &view));
    view.actor_count = 0;
    view.property_flash_adjust = view.lands[0].flash_adjust = -4;
    rich4_game_viewport_project(&view.viewport, 352, 640, &cx, &cy);
    SDL_FillRect(screen, NULL, 0x2104);
    draw_game_property_flash(screen, &assets, &settings, &view, NULL);
    for (y = 0; y < building.height; y++) for (x = 0; x < building.width; x++) {
        int sx = cx - building.x + x, sy = cy - building.y + y;
        if (sy >= cy - ground.y || sy < 40 || sx < 0 || sx >= 440) continue;
        if (data[building.data_offset + y * building.width + x] == 0) continue;
        /* c=4,a=-4 is 3 in the original LUT. */
        assert(pixel(screen, sx, sy) == map_resource_pixel(screen,
            (uint16_t)((3 << 10) | (3 << 5) | 3)));
        found++;
    }
    assert(found > 0);
    assets.map.map_object_data[1] = NULL;
    assets.map.has_map_object_sprite[1] = assets.map.map_object_resource_present[1] = 0;
    free(data);
}

void scalar_draw_game_property_flash(SDL_Surface *, const GamePreviewAssets *,
    const NewGameSettings *, const GameViewModel *, const GameMapDynamicObjectState *);

static void check_sequential_flash(void)
{
    static GameSession session;
    GamePropertyPresentationState *p = &session.presentation.property;
    int i;
    Uint32 now = 100;
    memset(&session, 0, sizeof(session));
    p->phase = GAME_PROPERTY_PRESENT_FLASH;
    assert(rich4_property_update_charge_presentation(&session, now));
    assert(p->flash_frame == 0 && p->flash_adjust == 4);
    rich4_property_flash_frame_presented(&session, now + 20);
    assert(rich4_property_update_charge_presentation(&session, now + 49));
    assert(p->flash_frame == 0); /* Thirty milliseconds after presentation. */
    now += 470; /* The real-device slow-render interval must not skip frames. */
    assert(rich4_property_update_charge_presentation(&session, now));
    assert(p->flash_frame == 1 && p->flash_adjust == 8);
    assert(rich4_property_update_charge_presentation(&session, now + 1));
    assert(p->flash_frame == 1);
    for (i = 2; i <= 16; i++) {
        now += 30;
        assert(rich4_property_update_charge_presentation(&session, now));
        assert(p->flash_frame == i);
    }
    assert(rich4_property_update_charge_presentation(&session, now + 399));
    assert(p->phase == GAME_PROPERTY_PRESENT_FLASH);
    assert(rich4_property_update_charge_presentation(&session, now + 400));
    assert(p->phase == GAME_PROPERTY_PRESENT_AMOUNT);
    memset(&session, 0, sizeof(session)); p->phase = GAME_PROPERTY_PRESENT_FLASH;
    p->flash_frame = 3; p->flash_adjust = 16;
    assert(rich4_property_skip_wait(&session, 100));
    assert(rich4_property_update_charge_presentation(&session, 499));
    assert(p->flash_adjust == 16); /* Interrupt holds the last drawn frame. */
    assert(rich4_property_update_charge_presentation(&session, 500));
    assert(p->phase == GAME_PROPERTY_PRESENT_AMOUNT && p->flash_adjust == 0);
    memset(&session, 0, sizeof(session)); p->phase = GAME_PROPERTY_PRESENT_FLASH;
    assert(rich4_property_skip_wait(&session, 100));
    assert(rich4_property_update_charge_presentation(&session, 499));
    assert(p->phase == GAME_PROPERTY_PRESENT_FLASH);
    assert(rich4_property_skip_wait(&session, 500));
    assert(rich4_property_update_charge_presentation(&session, 500));
    assert(p->phase == GAME_PROPERTY_PRESENT_AMOUNT);
    puts("QA_PROPERTY_CLOCK_PASS frames=16 frame_ms=30 hold_ms=400 stall_ms=470 skipped=0");
}

static void check_mask_equivalence_and_speed(SDL_Surface *screen, Rich4Mkf *mkf)
{
    static const int adjustments[16] = {4,8,12,16,12,8,4,0,-4,-8,-12,-16,-12,-8,-4,0};
    SDL_Surface *reference = SDL_CreateRGBSurface(SDL_SWSURFACE,640,480,16,0xf800,0x07e0,0x1f,0);
    SDL_Surface *base = SDL_CreateRGBSurface(SDL_SWSURFACE,640,480,16,0xf800,0x07e0,0x1f,0);
    unsigned char *building;
    size_t size;
    Uint32 old_ms = 0, new_ms = 0, cached_ms = 0, started;
    int rotation, frame, i, y;
    int scalar_comparisons = 0;
#ifdef RICH4_PLATFORM_MIYOO
    const int land_count = 8;
#else
    const int land_count = 32;
#endif
    GameRenderCache cache = {0};
    assert(reference && base);
    assert(rich4_mkf_read_resource(mkf,39,&building,&size,NULL)==RICH4_MKF_OK);
    assets.map.map_object_data[1] = building; assets.map.map_object_size[1] = size;
    assets.map.has_map_object_sprite[1] = assets.map.map_object_resource_present[1] = 1;
    assert(rich4_spr_smp_probe(building,size,&assets.map.map_object_spr[1]));
    setup(0);
    state.land_count = land_count;
    assets.map.maps[0].land_count = land_count + 1;
    for (i = 0; i < land_count; i++) {
        lands[i+1] = lands[1];
        lands[i+1].parity_overlay = 0;
        lands[i+1].x = 256 + (i % 8) * 32;
        lands[i+1].y = 576 + (i / 8) * 32;
        lands[i+1].variant = i & 1;
        state.land_owner[i] = i & 1;
        state.land_level[i] = 1;
    }
    for (rotation = 0; rotation < 8; rotation++) {
        input.map_direction = rotation;
        assert(rich4_game_view_model_build(&state,&input,&assets.map.maps[0],&view));
        view.actor_count = 2;
        for (i = 0; i < 2; i++) {
            view.actors[i].visible = 1;
            view.actors[i].world_x = 352 + i*32;
            view.actors[i].world_y = 640;
            view.actors[i].overlay_part = -1;
        }
        view.property_flash_adjust = 0;
        SDL_FillRect(base,NULL,0x2104);
        draw_game_map_objects(base,&assets,&settings,&view,NULL);
        view.property_flash_active = 1;
        for (frame = 0; frame < 16; frame++) {
            int compare_scalar = 1;
#ifdef RICH4_PLATFORM_MIYOO
            /* Native checks all 128 scalar frames with 32 lands. In QEMU,
             * compare both mask signs at every rotation, then compare all
             * cached frames against the verified stamp path. */
            compare_scalar = frame == 0 || frame == 8;
#endif
            view.property_flash_adjust = adjustments[frame];
            for (i = 0; i < land_count; i++) view.lands[i].flash_adjust = (i % 3) ? adjustments[frame] : 0;
            if (compare_scalar) {
                SDL_BlitSurface(base,NULL,reference,NULL);
                started = SDL_GetTicks();
                scalar_draw_game_property_flash(reference,&assets,&settings,&view,NULL);
                old_ms += SDL_GetTicks()-started;
                scalar_comparisons++;
            }
            SDL_BlitSurface(base,NULL,screen,NULL);
            started = SDL_GetTicks();
            draw_game_property_flash(screen,&assets,&settings,&view,NULL);
            new_ms += SDL_GetTicks()-started;
            if (compare_scalar) {
                for (y = 0; y < 480; y++)
                    assert(memcmp((char *)reference->pixels+y*reference->pitch,
                                  (char *)screen->pixels+y*screen->pitch,1280)==0);
            } else SDL_BlitSurface(screen,NULL,reference,NULL);
            SDL_BlitSurface(base,NULL,screen,NULL);
            started = SDL_GetTicks();
            assert(draw_game_cached_property_flash(screen,&assets,&settings,&view,NULL,&cache));
            cached_ms += SDL_GetTicks()-started;
            for (y = 0; y < 480; y++)
                assert(memcmp((char *)reference->pixels+y*reference->pitch,
                              (char *)screen->pixels+y*screen->pitch,1280)==0);
        }
        view.property_flash_active = 0;
        assert(!draw_game_cached_property_flash(screen,&assets,&settings,&view,NULL,&cache));
        assert(!cache.property_flash_base && !cache.property_flash_mask);
    }
    printf("QA_PROPERTY_MASK_PASS rotations=8 frames=128 scalar_comparisons=%d lands=%d occluding_actors=2 scalar_ms=%u stamp_ms=%u cached_ms=%u cache_bytes=580800\n",
        scalar_comparisons,land_count,old_ms,new_ms,cached_ms);
    free(building);
    assets.map.map_object_data[1] = NULL;
    assets.map.has_map_object_sprite[1] = assets.map.map_object_resource_present[1] = 0;
    SDL_FreeSurface(reference); SDL_FreeSurface(base);
}

/* Measure the original on-demand read/decompression path without repacking. */
static void check_first_resource_loading(void)
{
    Rich4Mkf *archive;
    const int indices[] = {553,544,569};
    int i;
    if (rich4_mkf_open("assets/original/Data.mkf",&archive) != RICH4_MKF_OK) {
        rich4_mkf_close(archive);
        assert(rich4_mkf_open("assets/original/Data.mkf",&archive)==RICH4_MKF_OK);
    }
    for (i = 0; i < 3; i++) {
        unsigned char *data; size_t size; Uint32 start = SDL_GetTicks();
        assert(rich4_mkf_read_resource(archive,indices[i],&data,&size,NULL)==RICH4_MKF_OK);
        printf("QA_ORIGINAL_RESOURCE index=%d bytes=%lu read_decode_ms=%u\n",
               indices[i],(unsigned long)size,SDL_GetTicks()-start);
        free(data);
    }
    rich4_mkf_close(archive);
}

int main(void)
{
    Rich4Mkf *mkf;
    assert(SDL_Init(SDL_INIT_TIMER) == 0);
    unsigned char *data;
    size_t size;
    SDL_Surface *screen = SDL_CreateRGBSurface(SDL_SWSURFACE, 640, 480, 16,
        0xf800, 0x07e0, 0x001f, 0);
    assert(screen != NULL);
    if (rich4_mkf_open("assets/original/Map.mkf", &mkf) != RICH4_MKF_OK) {
        rich4_mkf_close(mkf);
        assert(rich4_mkf_open("assets/original/Map.mkf", &mkf) == RICH4_MKF_OK);
    }
    assert(rich4_mkf_read_resource(mkf, 26, &data, &size, NULL) == RICH4_MKF_OK);
    assets.map.map_common_data[2] = data;
    assets.map.map_common_size[2] = size;
    assets.map.has_map_common_graph[2] = assets.map.has_map_common[2] = 1;
    assert(rich4_spr_smp_probe(data, size, &assets.map.map_common_spr[2]));
    assets.map.maps[0].lands = lands;
    assets.map.maps[0].land_count = 2;
    assets.map.maps[0].facilities = facilities;
    assets.map.maps[0].facility_count = 2;
    check_card_markers(screen);
    check_flash_footprint_and_occlusion(screen);
    check_flash_building_pixels(screen, mkf);
    check_sequential_flash();
    check_mask_equivalence_and_speed(screen,mkf);
    check_first_resource_loading();
    free(data);
    rich4_mkf_close(mkf);
    SDL_FreeSurface(screen);
    puts("property map: red/blue, eight rotations, five daily settlements, flash footprint and actor occlusion: ok");
    return 0;
}
