#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL/SDL.h>
#ifdef main
#undef main
#endif

#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/render/game_panel_render.h"
#include "platform/sdl1/render/game_flc_cache.h"
#include "platform/sdl1/render/game_map_render.h"

int reference_draw_game_main_map_view(SDL_Surface *, const GamePreviewAssets *,
    const NewGameSettings *, const GameViewModel *);
int reference_draw_game_graph_frame_step_data(SDL_Surface *, const unsigned char *,
    size_t, int, int, int, const unsigned char *, int);

static void background(SDL_Surface *screen, int seed)
{
    int x, y;
    for (y = 0; y < 480; y++) {
        uint16_t *row = (uint16_t *)((unsigned char *)screen->pixels + y * screen->pitch);
        for (x = 0; x < 640; x++) row[x] = (uint16_t)(x * 73 + y * 137 + seed * 1031);
    }
}

static void equal_frames(SDL_Surface *actual, SDL_Surface *reference,
    const char *kind, int frame)
{
    int y;
    for (y = 0; y < 480; y++) {
        if (memcmp((unsigned char *)actual->pixels + y * actual->pitch,
                   (unsigned char *)reference->pixels + y * reference->pitch, 1280)) {
            fprintf(stderr, "pixel mismatch: %s frame=%d row=%d\n", kind, frame, y);
            abort();
        }
    }
}

static void compare_flc(SDL_Surface *a, SDL_Surface *b, SDL_Surface *source,
    const unsigned char *data, size_t size, int frame, int x, int y)
{
    background(a, frame + 5);
    background(b, frame + 5);
    background(source, frame + 17); /* Different from destination, changes every draw. */
    assert(reference_draw_game_graph_frame_step_data(b, data, size, frame,
        x, y, source->pixels, source->pitch));
    assert(draw_game_graph_frame_step_data(a, data, size, frame,
        x, y, source->pixels, source->pitch));
    equal_frames(a, b, "FLC", frame);
}

static void check_flc(GamePreviewAssets *assets, SDL_Surface *a, SDL_Surface *b,
    SDL_Surface *source)
{
    static const struct { const char *archive; int index; } cases[] = {
        {"Jump",46}, {"Jump",47}, {"Jump",59}, {"Data",559},
        {"Data",544}, {"Panel",20}, {"Panel",14}, {"Panel",16}, {"Panel",78}
    };
    int test;
    for (test = 0; test < 9; test++) {
        char path[128];
        Rich4Mkf *mkf;
        Rich4FlcHeader header;
        unsigned char *data;
        size_t size;
        int frame, count;
        GameFlcCacheStats before, after;
        snprintf(path, sizeof(path), "assets/original/%s.mkf", cases[test].archive);
        assert(rich4_resource_store_open_archive(assets->resource_store, path, &mkf) == RICH4_MKF_OK);
        assert(game_preview_borrow_resource(assets, mkf, cases[test].index, &data, &size, NULL) == RICH4_MKF_OK);
        if (rich4_flc_parse_header(data, size, &header) != RICH4_FLC_OK) {
            fprintf(stderr, "not FLC: %s[%d] size=%lu magic=%02x%02x\n", cases[test].archive,
                cases[test].index, (unsigned long)size, data[4], data[5]);
            abort();
        }
        count = header.frame_count < 60 ? header.frame_count : 60;
        before = game_flc_cache_stats();
        for (frame = 0; frame < count; frame++) {
            game_flc_cache_begin_frame(assets);
            compare_flc(a, b, source, data, size, frame, -23, 15);
            compare_flc(a, b, source, data, size, frame, 120, -17);
        }
        after = game_flc_cache_stats();
        assert(after.decoded_frames - before.decoded_frames == (unsigned long)count);
        assert(after.hits > before.hits);
        compare_flc(a, b, source, data, size, header.frame_count - 1, 0, 0);
        compare_flc(a, b, source, data, size, 0, 510, 430); /* Rewind and clipping. */
        assert(game_flc_cache_stats().live_bytes <= 8u * 1024u * 1024u);
        printf("FLC equivalent: %s[%d] sequential=%d repeat/skip/rewind/background/clip=ok\n",
            cases[test].archive, cases[test].index, count);
    }
    game_flc_cache_begin_frame(assets);
    game_flc_cache_begin_frame(assets);
    game_flc_cache_begin_frame(assets);
    assert(game_flc_cache_stats().live_bytes == 0);
    {
        size_t resource;
        int streams = 0;
        unsigned long evictions = game_flc_cache_stats().evictions;
        /* Nine concurrent streams exercise slot eviction, not only idle expiry. */
        for (resource = 0; resource < assets->borrowed_resource_count; resource++) {
            Rich4ResourceHandle handle = assets->borrowed_resources[resource];
            Rich4FlcHeader header;
            if (rich4_flc_parse_header(handle.data, handle.size, &header) != RICH4_FLC_OK) continue;
            assert(game_flc_cache_decode(handle.data, handle.size, 0) != NULL);
            streams++;
            assert(game_flc_cache_stats().live_bytes <= 8u * 1024u * 1024u);
        }
        assert(streams == 9 && game_flc_cache_stats().evictions > evictions);
        assert(game_flc_cache_stats().live_bytes > 0);
        puts("FLC cache bound: 9 streams, eviction=ok, idle expiry=ok");
    }
}

static void check_ground_coverage(SDL_Surface *screen)
{
    const size_t size = 0x2a90u + 1024u;
    GamePreviewAssets *assets = calloc(1, sizeof(*assets));
    GameViewModel *view = calloc(1, sizeof(*view));
    unsigned char *data = calloc(1, size);
    NewGameSettings settings = {0};
    int direction, repeat, x, y;
    assert(assets && view && data);
    memcpy(data, "GND", 3);
    data[4] = data[6] = 72;
    /* One white tile makes every missing pixel visible, independently of
     * the scalar reference's traversal and the commercial map artwork. */
    data[0x12] = 0xff; data[0x13] = 0x7f;
    memset(data + 0x2a90, 1, 1024);
    assets->map.map_ground_resource[0].data = data;
    assets->map.map_ground_resource[0].size = size;
    assets->map.has_map_ground[0] = 1;
    view->active = view->actor_count = 1;
    view->viewport.center_world_x = view->viewport.center_world_y = 32 * 36;
    game_reset_ground_render_cache();
    for (direction = 0; direction < 8; direction++) {
        view->viewport.direction = direction;
        for (repeat = 0; repeat < 2; repeat++) {
            uint16_t untouched = (uint16_t)(0x1234 + repeat);
            SDL_FillRect(screen, NULL, untouched);
            assert(draw_game_main_map_view(screen, assets, &settings, view));
            for (y = 0; y < 480; y++) {
                const uint16_t *row = (const uint16_t *)
                    ((const unsigned char *)screen->pixels + y * screen->pitch);
                for (x = 0; x < 640; x++) {
                    uint16_t expected = x < 440 && y >= 40 ? 0xffdf : untouched;
                    if (row[x] != expected) {
                        fprintf(stderr, "ground coverage: direction=%d "
                            "repeat=%d pixel=(%d,%d) actual=%04x expected=%04x\n",
                            direction, repeat, x, y, row[x], expected);
                        abort();
                    }
                }
            }
        }
    }
    game_reset_ground_render_cache();
    free(data); free(view); free(assets);
    puts("ground coverage: 8 directions x tile-aligned camera x miss/hit=16, viewport/clip=ok");
}

static void check_ground(GamePreviewAssets *assets, SDL_Surface *a, SDL_Surface *b)
{
    static const int centers[][2] = {{1100,1100},{1101,1131},{0,0},{2303,2303},{-31,32}};
    NewGameSettings settings = {0};
    GameViewModel *view = calloc(1, sizeof(*view));
    Rich4Mkf *mkf;
    unsigned char *data;
    size_t size;
    int map, direction, camera, repeat;
    assert(view != NULL);
    view->active = 1; view->actor_count = 1;
    assert(rich4_resource_store_open_archive(assets->resource_store,
        "assets/original/Map.mkf", &mkf) == RICH4_MKF_OK);
    for (map = 0; map < 8; map += 7) {
        assert(game_preview_borrow_resource(assets, mkf, map * 2, &data, &size, NULL) == RICH4_MKF_OK);
        assets->map.map_ground_resource[map] = game_preview_handle_for_borrowed_view(assets, data);
        assets->map.has_map_ground[map] = 1;
        settings.stage = map / 4; settings.map = map % 4;
        for (direction = 0; direction < 8; direction++) {
            view->viewport.direction = direction;
            for (camera = 0; camera < 5; camera++) {
                view->viewport.center_world_x = centers[camera][0];
                view->viewport.center_world_y = centers[camera][1];
                for (repeat = 0; repeat < 2; repeat++) {
                    background(a, repeat + camera * 17);
                    background(b, repeat + camera * 17);
                    assert(reference_draw_game_main_map_view(b, assets, &settings, view));
                    assert(draw_game_main_map_view(a, assets, &settings, view));
                    equal_frames(a, b, "ground", direction * 10 + camera);
                }
            }
        }
    }
    free(view);
    puts("ground equivalent: 2 maps x 8 directions x 5 cameras x miss/hit=160");
}

static int check_demand_panels(SDL_Surface *actual, SDL_Surface *reference)
{
    GamePreviewAssets *lazy=calloc(1,sizeof(*lazy));
    GamePreviewAssets *direct=calloc(1,sizeof(*direct));
    ResourceStore *store=rich4_resource_store_create();
    SdlOptions options={0};
    int resource, comparisons=0;
    assert(lazy && direct && store);
    options.data_dir="assets/original";
    assert(game_load_scoped_preview_assets(store,&options,lazy,2,0,3));
    for(resource=0;resource<GAME_PANEL_RESOURCE_COUNT;resource++) {
        unsigned char *data=NULL;
        size_t size=0;
        Rich4FlcHeader header;
        assert(lazy->ui.panel_data[resource]==NULL);
        assert(rich4_mkf_read_resource(lazy->panel_archive,(size_t)resource,
                                      &data,&size,NULL)==RICH4_MKF_OK);
        game_flc_cache_begin_frame(lazy);
        assert(game_asset_require_data(lazy,&lazy->ui.panel_data[resource]));
        assert(lazy->ui.panel_size[resource]==size);
        assert(memcmp(data,lazy->ui.panel_data[resource],size)==0);
        direct->ui.panel_data[resource]=data;
        direct->ui.panel_size[resource]=size;
        if(rich4_spr_smp_probe(data,size,&direct->ui.panel_spr[resource]) &&
           direct->ui.panel_spr[resource].num_chunks>0) {
            unsigned int chunks=direct->ui.panel_spr[resource].num_chunks;
            unsigned int choices[3]={0,chunks/2,chunks-1};
            int i;
            direct->ui.has_panel[resource]=1;
            for(i=0;i<3;i++) {
                background(actual,i+resource); background(reference,i+resource);
                assert(draw_game_panel_chunk_at(actual,lazy,resource,choices[i],320,240,1));
                assert(draw_game_panel_chunk_at(reference,direct,resource,choices[i],320,240,1));
                equal_frames(actual,reference,"demand panel",resource);
                comparisons++;
            }
        } else if(rich4_flc_parse_header(data,size,&header)==RICH4_FLC_OK) {
            int frame;
            for(frame=0;frame<3 && frame<header.frame_count;frame++) {
                background(actual,frame+resource); background(reference,frame+resource);
                assert(draw_game_panel_graph_frame(actual,lazy,resource,frame,10,15));
                assert(reference_draw_game_graph_frame_step_data(reference,data,size,frame,
                    10,15,reference->pixels,reference->pitch));
                equal_frames(actual,reference,"demand FLC",resource);
                comparisons++;
            }
        }
        game_lazy_assets_begin_frame(lazy);
        game_lazy_assets_begin_frame(lazy);
        assert(lazy->ui.panel_data[resource]==NULL);
        assert(!lazy->ui.has_panel[resource] && !lazy->ui.has_panel_graph[resource]);
        assert(game_asset_require_data(lazy,&lazy->ui.panel_data[resource]));
        assert(memcmp(data,lazy->ui.panel_data[resource],size)==0);
        free(data);
        game_lazy_assets_begin_frame(lazy);
        game_lazy_assets_begin_frame(lazy);
    }
    game_free_preview_assets(lazy);
    assert(rich4_resource_store_cached_bytes(store)==0);
    rich4_resource_store_destroy(store);
    free(lazy); free(direct);
    printf("demand panels equivalent: 113 resources, %d full-frame comparisons, release/reload ok\n",comparisons);
    return comparisons;
}

int main(void)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    GamePreviewAssets *assets = calloc(1, sizeof(*assets));
    SDL_Surface *a, *b, *source;
    GameFlcCacheStats stats;
    int demand_comparisons;
    assert(assets != NULL);
    assets->resource_store = rich4_resource_store_create();
    assert(assets->resource_store != NULL);
    a = SDL_CreateRGBSurface(SDL_SWSURFACE, 640, 480, 16, 0xf800, 0x07e0, 0x001f, 0);
    b = SDL_CreateRGBSurface(SDL_SWSURFACE, 640, 480, 16, 0xf800, 0x07e0, 0x001f, 0);
    source = SDL_CreateRGBSurface(SDL_SWSURFACE, 640, 480, 16, 0xf800, 0x07e0, 0x001f, 0);
    assert(a && b && source);
    check_ground_coverage(a);
    check_ground(assets, a, b);
    check_flc(assets, a, b, source);
    stats = game_flc_cache_stats();
    printf("FLC cache test: hits=%lu decoded=%lu peak_bytes=%lu\n", stats.hits,
        stats.decoded_frames, (unsigned long)stats.peak_bytes);
    demand_comparisons=check_demand_panels(a,b);
    {
        FILE *report = fopen("build/render-equivalence.json", "wb");
        assert(report != NULL);
        fprintf(report, "{\"passed\":true,\"ground_comparisons\":160,\"ground_coverage_checks\":16,\"flc_resources\":9,\"cache_peak_bytes\":%lu,\"decoded_frames\":%lu,\"cache_evictions\":%lu,\"demand_panel_resources\":113,\"demand_pixel_comparisons\":%d}\n",
            (unsigned long)stats.peak_bytes, stats.decoded_frames, stats.evictions,demand_comparisons);
        fclose(report);
    }
    {
        ResourceStore *store = assets->resource_store;
        game_free_preview_assets(assets);
        assert(game_flc_cache_stats().live_bytes == 0);
        rich4_resource_store_destroy(store);
    }
    free(assets);
    SDL_FreeSurface(a); SDL_FreeSurface(b); SDL_FreeSurface(source);
    puts("render pixel equivalence: PASS");
    return 0;
}
