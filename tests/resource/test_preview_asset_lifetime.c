#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/game_asset_api.h"
#include "resource/resource_store.h"

#ifdef main
#undef main
#endif

/* This isolated lease test has no renderer. The SDL equivalence test covers
 * the real derived-cache retirement before releasing owner resources. */
void game_flc_cache_clear(void) {}
void game_reset_ground_render_cache(void) {}
int game_lazy_asset_deferred(const GamePreviewAssets *a, const void *slot)
{ (void)a; (void)slot; return 0; }
void game_lazy_assets_free(GamePreviewAssets *a) { (void)a; }
Rich4ResourceHandle game_lazy_asset_handle(const GamePreviewAssets *a,
                                         const unsigned char *data)
{
    Rich4ResourceHandle empty;
    (void)a; (void)data;
    memset(&empty,0,sizeof(empty));
    return empty;
}

static void write_u32le(FILE *fp, uint32_t value)
{
    fputc((int)(value & 0xff), fp);
    fputc((int)((value >> 8) & 0xff), fp);
    fputc((int)((value >> 16) & 0xff), fp);
    fputc((int)((value >> 24) & 0xff), fp);
}

static void make_robot_archive(const char *path)
{
    enum { RESOURCE_COUNT = 523 };
    const uint32_t first_resource = 4;
    const uint32_t resource_size = 20;
    const uint32_t index_offset = first_resource + RESOURCE_COUNT * resource_size;
    FILE *fp = fopen(path, "wb");
    int i;
    assert(fp != NULL);
    write_u32le(fp, index_offset);
    for (i = 0; i < RESOURCE_COUNT; i++) {
        write_u32le(fp, 4);
        write_u32le(fp, 4);
        write_u32le(fp, 0);
        write_u32le(fp, 4);
        fwrite(i == 521 ? "ROBO" : i == 522 ? "DOLL" : "----", 1, 4, fp);
    }
    for (i = 0; i < RESOURCE_COUNT; i++)
        write_u32le(fp, first_resource + (uint32_t)i * resource_size);
    assert(fclose(fp) == 0);
}

int main(void)
{
    const char *path = "build/preview_asset_lifetime.mkf";
    ResourceStore *store;
    Rich4Mkf *mkf = NULL;
    GamePreviewAssets assets;
    unsigned char *robot = NULL;
    unsigned char *doll = NULL;
    size_t robot_size = 0;
    size_t doll_size = 0;

    make_robot_archive(path);
    memset(&assets, 0, sizeof(assets));
    store = rich4_resource_store_create();
    assert(store != NULL);
    assets.resource_store = store;
    assert(rich4_resource_store_open_archive(store, path, &mkf) == RICH4_MKF_OK);
    assert(game_preview_borrow_resource(&assets, mkf, 521, &robot,
                                        &robot_size, NULL) == RICH4_MKF_OK);
    assert(game_preview_borrow_resource(&assets, mkf, 522, &doll,
                                        &doll_size, NULL) == RICH4_MKF_OK);
    assert(robot_size == 4 && doll_size == 4);
    assert(memcmp(robot, "ROBO", 4) == 0);
    assert(memcmp(doll, "DOLL", 4) == 0);
    assets.character.tool_robot[0].data = robot;
    assets.character.tool_robot[1].data = doll;
    assets.character.tool_robot[0].size = robot_size;
    assets.character.tool_robot[1].size = doll_size;

    /* Preview teardown only clears borrowed aliases.  The store remains the
     * sole owner and releases both decoded resources exactly once below. */
    game_free_preview_assets(&assets);
    rich4_resource_store_destroy(store);
    assert(remove(path) == 0);
    puts("preview asset ownership lifetime: ok");
    return 0;
}
