#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "platform/sdl1/game_lazy_assets.h"
#include "platform/sdl1/game_event_presentation.h"
#include "platform/sdl1/render/game_flc_cache.h"
#include "resource/byte_reader.h"
#include "resource/flc_animation.h"
#include "view/view_constants.h"

/* Archive/slot mappings are the same as game_preview_loader.c and
 * game_character_assets.c. Original ownership anchors: 0x4190CF (Panel 0..8),
 * 0x418C55 (Data 559+c), 0x40B0CD (Data 523), 0x43380A (Panel 18..20),
 * 0x415872 (Jump 45..70), and resource_catalog.c's per-family call sites.
 * A lease lasts through every frame that actually consumes its pixels.
 * Timing queries keep the active sequence leased, as in 0x415872, so its
 * first draw does not have to decompress the same resource again. */
enum { LAZY_MAX = 1024, LAZY_KEYS = LAZY_MAX * 5, WARM_MAX = 64 };
enum Archive { DATA, PANEL, JUMP };
enum Format { SPR, DIRECTIONAL, FLC, RAW, RAW_EXACT, EVENT_IMAGE, PANEL_FORMAT };
typedef struct LazyAsset {
    unsigned char **data;
    size_t *size;
    Rich4SprSmpInfo *spr;
    int *flags[4], *present;
    Rich4ResourceHandle handle;
    unsigned long used_frame;
    size_t index, minimum;
    int archive, format, known, frames, delay, failed, prepared;
} LazyAsset;
typedef struct WarmAsset { Rich4ResourceHandle handle; unsigned long age; } WarmAsset;
typedef struct LazyKey { const void *slot; unsigned short entry; } LazyKey;
struct GameLazyAssets {
    LazyAsset entries[LAZY_MAX];
    LazyKey keys[LAZY_KEYS];
    size_t count, key_count;
    unsigned long frame;
    WarmAsset warm[WARM_MAX];
    size_t warm_bytes;
    unsigned long warm_clock;
};

static Rich4Mkf *archive_for(const GamePreviewAssets *a, int archive)
{
    return archive == DATA ? a->data_archive :
        archive == PANEL ? a->panel_archive : a->jump_archive;
}

static int key_compare(const void *left, const void *right)
{
    uintptr_t a = (uintptr_t)((const LazyKey *)left)->slot;
    uintptr_t b = (uintptr_t)((const LazyKey *)right)->slot;
    return (a > b) - (a < b);
}

static LazyAsset *lookup(const GamePreviewAssets *a, const void *slot)
{
    LazyKey key, *found;
    if (a == NULL || a->lazy == NULL || slot == NULL) return NULL;
    key.slot = slot;
    found = bsearch(&key, a->lazy->keys, a->lazy->key_count,
                    sizeof(key), key_compare);
    return found != NULL ? &a->lazy->entries[found->entry] : NULL;
}

static void add_key(struct GameLazyAssets *l, const void *slot, size_t entry)
{
    if (slot == NULL) return;
    l->keys[l->key_count].slot = slot;
    l->keys[l->key_count++].entry = (unsigned short)entry;
}

static LazyAsset *bind_asset(GamePreviewAssets *a, int archive, size_t index,
    unsigned char **data, size_t *size, int *flag, Rich4SprSmpInfo *spr,
    int format, size_t minimum)
{
    struct GameLazyAssets *l = a->lazy;
    LazyAsset *e = &l->entries[l->count];
    e->archive = archive; e->index = index;
    e->data = data; e->size = size; e->flags[0] = flag; e->spr = spr;
    e->format = format; e->minimum = minimum;
    add_key(l, data, l->count);
    add_key(l, flag, l->count);
    l->count++;
    return e;
}

int game_lazy_assets_init(GamePreviewAssets *a)
{
    static const int calendar[8] = {4,28,47,67,87,108,95,118};
    static const int tools[10] = {524,525,528,530,553,538,533,558,532,552};
    static const int consequences[5] = {531,539,527,534,535};
    static const int gods[19] = {0,540,541,542,543,544,545,546,547,548,549,0,550,0,0,551,0,0,0};
    int i, j;
    LazyAsset *e;
    if (a->load_scope != 2) return 1;
    a->lazy = calloc(1, sizeof(*a->lazy));
    if (a->lazy == NULL) return 0;
#define BIND(ar,idx,d,s,f,p,fmt,min) bind_asset(a,ar,(size_t)(idx),&a->d,&a->s,&a->f,p,fmt,min)
#define GRAPH(ar,idx,d,s,f) BIND(ar,idx,d,s,f,NULL,FLC,0)
#define SPRITE(ar,idx,d,s,f,p,min) BIND(ar,idx,d,s,f,&a->p,SPR,min)
    for (i = 0; i < GAME_PANEL_RESOURCE_COUNT; i++) {
        e = BIND(PANEL,i,ui.panel_data[i],ui.panel_size[i],ui.has_panel[i],
                 &a->ui.panel_spr[i],PANEL_FORMAT,0);
        e->flags[1] = &a->ui.has_panel_graph[i];
        e->flags[2] = &a->ui.has_panel_raw[i];
        e->flags[3] = i < 9 ? &a->ui.has_panel_mask[i] : NULL;
        for (j = 1; j < 4; j++) add_key(a->lazy,e->flags[j],a->lazy->count-1);
    }
    SPRITE(PANEL,77,menu.ai_settings_data,menu.ai_settings_size,
           menu.has_ai_settings_ui,menu.ai_settings_spr,18);
    SPRITE(PANEL,72,ui.remote_dice_data,ui.remote_dice_size,
           ui.has_remote_dice_ui,ui.remote_dice_spr,7);
    SPRITE(DATA,3,menu.options_data,menu.options_size,menu.has_options_ui,menu.options_spr,16);
    SPRITE(DATA,440,ui.confirm_dialog_data,ui.confirm_dialog_size,ui.has_confirm_dialog,ui.confirm_dialog_spr,3);
    SPRITE(DATA,517,ui.common_dialog_data,ui.common_dialog_size,ui.has_common_dialog,ui.common_dialog_spr,7);
    SPRITE(DATA,520,ui.save_load_panel_data,ui.save_load_panel_size,ui.has_save_load_panel,ui.save_load_panel_spr,11);
    SPRITE(DATA,2,ui.save_load_icons_data,ui.save_load_icons_size,ui.has_save_load_icons,ui.save_load_icons_spr,1);
    SPRITE(DATA,519,ui.speech_icon_data,ui.speech_icon_size,ui.has_speech_icons,ui.speech_icon_spr,1);
    SPRITE(DATA,517,ui.worker_choice_data,ui.worker_choice_size,ui.has_worker_choice_ui,ui.worker_choice_spr,18);
    SPRITE(DATA,518,ui.player_choice_data,ui.player_choice_size,ui.has_player_choice,ui.player_choice_spr,1);
    GRAPH(DATA,523,tool.property_max_level_data,tool.property_max_level_size,tool.has_property_max_level_graph);
    GRAPH(DATA,557,ui.monster_card_data,ui.monster_card_size,ui.has_monster_card_animation);
    GRAPH(DATA,529,ui.demolish_card_data,ui.demolish_card_size,ui.has_demolish_card_animation);
    GRAPH(DATA,555,ui.bankruptcy_data,ui.bankruptcy_size,ui.has_bankruptcy_animation);
    GRAPH(DATA,556,ui.ending_success_data,ui.ending_success_size,ui.has_ending_success_animation);
    GRAPH(DATA,526,ui.point_minigame_effect_data,ui.point_minigame_effect_size,ui.has_point_minigame_effect);
    for (i = 0; i < GAME_TOOL_EFFECT_ASSET_COUNT; i++)
        GRAPH(DATA,tools[i],tool.tool_effect_data[i],tool.tool_effect_size[i],tool.has_tool_effect[i]);
    for (i = 0; i < 2; i++)
        GRAPH(DATA,536+i,ui.special_reward_data[i],ui.special_reward_size[i],ui.has_special_reward[i]);
    for (i = 1; i <= GAME_GOD_TYPE_COUNT; i++) if (gods[i])
        GRAPH(DATA,gods[i],ui.god_effect_data[i],ui.god_effect_size[i],ui.has_god_effect[i]);
    for (i = 0; i < GAME_SPECIAL_EVENT_ASSET_COUNT; i++)
        BIND(DATA,441+i,ui.special_event_data[i],ui.special_event_size[i],ui.has_special_event[i],NULL,EVENT_IMAGE,0);
    for (i = 0; i < GAME_NEWS_CONSEQUENCE_COUNT; i++)
        GRAPH(DATA,consequences[i],ui.news_consequence_data[i],ui.news_consequence_size[i],ui.has_news_consequence[i]);
    for (i = 0; i < GAME_CARD_COUNT; i++)
        BIND(DATA,571+i,ui.card_effect_data[i],ui.card_effect_size[i],ui.has_card_effect[i],NULL,RAW,
             ORIGINAL_CARD_ART_WIDTH*ORIGINAL_CARD_ART_HEIGHT*2u);
    for (i = 0; i < 12; i++) for (j = 0; j < 2; j++)
        bind_asset(a,DATA,416+2*i+j,&a->ui.monthly_award_data[i][j],
                   &a->ui.monthly_award_size[i][j],NULL,NULL,FLC,0);
    i = a->active_background;
    if (i >= 0 && i < GAME_BACKGROUND_COUNT)
        for (j = 0; j < GAME_BANK_CALENDAR_EVENT_COUNT; j++)
            BIND(DATA,calendar[i]+j,ui.bank_calendar_data[i][j],ui.bank_calendar_size[i][j],
                 ui.has_bank_calendar[i][j],NULL,RAW_EXACT,200*200*2u);
    SPRITE(JUMP,45,intro.jump_intro_scene_data,intro.jump_intro_scene_size,
           intro.has_jump_intro_scene,intro.jump_intro_scene_spr,15);
    GRAPH(JUMP,46,intro.jump_intro_window_data,intro.jump_intro_window_size,intro.has_jump_intro_window_graph);
    for (i = 0; i < 12; i++) if (a->active_characters & (1u << i)) {
        GRAPH(DATA,559+i,character.first_landing_data[i],character.first_landing_size[i],character.has_first_landing_graph[i]);
        for (j = 0; j < 2; j++)
            GRAPH(JUMP,47+12*j+i,character.jump_intro_actor_data[j][i],character.jump_intro_actor_size[j][i],character.has_jump_intro_actor_graph[j][i]);
        for (j = 0; j < GAME_CHARACTER_MODEL_RESOURCES; j++) {
            e = BIND(DATA,128+21*i+j,character.map_player_data[i][j],character.map_player_size[i][j],
                     character.has_map_player[i][j],&a->character.map_player_spr[i][j],DIRECTIONAL,8);
            e->present = &a->character.map_player_resource_present[i][j];
        }
    }
    for (i = 0; i < GAME_SPECIAL_ACTOR_MODEL_COUNT; i++)
        for (j = 0; j < GAME_SPECIAL_ACTOR_MODEL_RESOURCES; j++) {
            e = BIND(DATA,380+i*GAME_SPECIAL_ACTOR_MODEL_RESOURCES+j,
                     character.map_special_actor_data[i][j],character.map_special_actor_size[i][j],
                     character.has_map_special_actor[i][j],&a->character.map_special_actor_spr[i][j],DIRECTIONAL,8);
            e->present = &a->character.map_special_actor_resource_present[i][j];
        }
    /* The fixed descriptor capacity also accommodates all twelve characters
     * in an explicit resource audit. */
    qsort(a->lazy->keys,a->lazy->key_count,sizeof(LazyKey),key_compare);
#undef SPRITE
#undef GRAPH
#undef BIND
    return 1;
}

void game_lazy_assets_finish(GamePreviewAssets *a)
{
    size_t i;
    if (a->lazy == NULL) return;
    for (i = 0; i < a->lazy->count; i++) {
        LazyAsset *e = &a->lazy->entries[i];
        Rich4MkfResourceInfo info;
        if (e->present != NULL)
            *e->present = rich4_mkf_get_info(archive_for(a,e->archive),e->index,&info) == RICH4_MKF_OK &&
                info.uncompressed_size != 0;
    }
}

int game_lazy_asset_deferred(const GamePreviewAssets *a, const void *slot)
{
    return lookup(a,slot) != NULL;
}

/* Retain immutable bytes only. The FLC cursor and public asset slots still
 * reset on release, preserving the original event start/stop behavior. */
static void retain_recent(GamePreviewAssets *a, LazyAsset *e)
{
    struct GameLazyAssets *l = a->lazy;
    size_t i, slot = WARM_MAX;
    if (e->archive == JUMP || e->format == DIRECTIONAL ||
        e->handle.size > GAME_LAZY_RECENT_BYTES) return;
    for (;;) {
        size_t oldest = WARM_MAX;
        for (i = 0; i < WARM_MAX; i++) {
            if (!l->warm[i].handle.generation) { slot = i; continue; }
            if (oldest == WARM_MAX || l->warm[i].age < l->warm[oldest].age) oldest = i;
        }
        if (slot != WARM_MAX && l->warm_bytes + e->handle.size <= GAME_LAZY_RECENT_BYTES) break;
        if (oldest == WARM_MAX) return;
        l->warm_bytes -= l->warm[oldest].handle.size;
        rich4_resource_store_release(a->resource_store, &l->warm[oldest].handle);
        slot = oldest;
    }
    l->warm[slot].handle = e->handle;
    l->warm[slot].age = ++l->warm_clock;
    l->warm_bytes += e->handle.size;
    memset(&e->handle, 0, sizeof(e->handle));
}

static void release_asset(GamePreviewAssets *a, LazyAsset *e, int retain)
{
    int i;
    if (!e->handle.generation) return;
    game_flc_cache_release_resource(e->handle.data);
    *e->data = NULL; *e->size = 0;
    for (i = 0; i < 4; i++) if (e->flags[i]) *e->flags[i] = 0;
    if (retain) retain_recent(a,e);
    rich4_resource_store_release(a->resource_store,&e->handle);
}

static int validate(LazyAsset *e)
{
    const unsigned char *data = *e->data;
    size_t size = *e->size;
    Rich4FlcHeader flc;
    Rich4SprSmpInfo spr;
    int valid = 0;
    e->frames = 0; e->delay = 0;
    if (rich4_flc_parse_header(data,size,&flc) == RICH4_FLC_OK) {
        e->frames = flc.frame_count; e->delay = (int)flc.delay_ms;
    }
    if (e->format == PANEL_FORMAT) {
        if (rich4_spr_smp_probe(data,size,e->spr) && e->spr->num_chunks > 0) {
            *e->flags[0] = 1; return 1;
        }
        if (((e->index >= 4 && e->index <= 6) || e->index == 14 ||
             e->index == 16 || e->index == 17 || e->index == 20 || e->index == 78) && e->frames > 0)
            *e->flags[1] = 1;
        else if ((e->index == 19 || e->index == 22 || e->index == 81 || e->index == 92) &&
                 size >= (e->index == 22 ? 128u*192u :
                    e->index == 92 ? 640u*480u*2u : 640u*480u))
            *e->flags[2] = 1;
        else if (e->index == 8 && size >= GAME_DICE_STATUS_W*GAME_DICE_STATUS_H)
            *e->flags[3] = 1;
        return *e->flags[1] || *e->flags[2] || (e->flags[3] && *e->flags[3]);
    }
    if (e->format == SPR || e->format == DIRECTIONAL) {
        valid = rich4_spr_smp_probe(data,size,&spr) && spr.num_chunks >= e->minimum;
        if (e->format == DIRECTIONAL) valid = valid && (spr.num_chunks & 7u) == 0;
        if (e->archive == PANEL && e->index == 77) valid = valid && memcmp(spr.signature,"SMP",4) == 0;
        if (valid && e->spr != NULL) *e->spr = spr;
    } else if (e->format == FLC) valid = e->frames > 0;
    else if (e->format == RAW) valid = size >= e->minimum;
    else if (e->format == RAW_EXACT) valid = size == e->minimum;
    else if (e->format == EVENT_IMAGE) {
        uint16_t w=0,h=0;
        valid = size == 388u*251u*2u || e->frames > 0 ||
            (rich4_byte_reader_le16(data,size,0,&w) && rich4_byte_reader_le16(data,size,2,&h) &&
             w && h && size >= 4u+(size_t)w*h*2u);
    }
    if (e->flags[0] != NULL) *e->flags[0] = valid;
    return valid;
}

static int acquire_asset(const GamePreviewAssets *a, LazyAsset *e)
{
    if (e->handle.generation) return 1;
    if (e->failed || rich4_resource_store_acquire(a->resource_store,
            archive_for(a,e->archive),e->index,&e->handle,NULL) != RICH4_MKF_OK) return 0;
    /* Acquire first so a matching retained lease becomes a store hit. */
    {
        size_t i;
        for (i = 0; i < WARM_MAX; i++) {
            WarmAsset *w = &a->lazy->warm[i];
            if (w->handle.generation && w->handle.data == e->handle.data) {
                a->lazy->warm_bytes -= w->handle.size;
                rich4_resource_store_release(a->resource_store, &w->handle);
            }
        }
    }
    *e->data = (unsigned char *)e->handle.data; *e->size = e->handle.size;
    e->known = 1;
    if (!validate(e)) {
        release_asset((GamePreviewAssets *)a,e,0); e->failed = 1; return 0;
    }
    return 1;
}

int game_asset_ready(const GamePreviewAssets *a, const int *flag)
{
    LazyAsset *e = lookup(a,flag);
    if (e == NULL) return flag != NULL && *flag;
    if (!acquire_asset(a,e)) return 0;
    e->used_frame = a->lazy->frame;
    e->prepared = 0;
    return *flag;
}

int game_asset_require_data(const GamePreviewAssets *a, const void *slot)
{
    LazyAsset *e = lookup(a,slot);
    if (e == NULL) return slot != NULL && *(const unsigned char *const *)slot != NULL;
    if (!acquire_asset(a,e)) return 0;
    e->used_frame = a->lazy->frame;
    e->prepared = 0;
    return 1;
}

static int graph_metadata(const GamePreviewAssets *a, const void *slot, int delay)
{
    LazyAsset *e = lookup(a,slot);
    if (e != NULL) {
        if (!acquire_asset(a,e)) return 0;
        e->used_frame = a->lazy->frame;
        e->prepared = 0;
        return delay ? e->delay : e->frames;
    }
    return -1;
}

int game_asset_graph_frames(const GamePreviewAssets *a, const void *slot, size_t size)
{
    int value = graph_metadata(a,slot,0);
    Rich4FlcHeader h;
    if (value >= 0) return value;
    return slot && rich4_flc_parse_header(*(const unsigned char *const *)slot,size,&h) == RICH4_FLC_OK
        ? h.frame_count : 0;
}
int game_asset_graph_delay(const GamePreviewAssets *a, const void *slot, size_t size)
{
    int value = graph_metadata(a,slot,1);
    Rich4FlcHeader h;
    if (value < 0) value = slot && rich4_flc_parse_header(*(const unsigned char *const *)slot,size,&h) == RICH4_FLC_OK
        ? (int)h.delay_ms : 0;
    return value > 0 ? value : GAME_INTRO_DEFAULT_FRAME_MS;
}

void game_lazy_assets_prepare_intro(GamePreviewAssets *a)
{
    int phase, character;
    /* 0x4190CF: shared map/dice Panel[0..8]. Prepare the persistent HUD
     * and selected first-landing resources (0x418C55) behind loading.
     * Their preparation lease ends at first use, then normal demand rules apply. */
    static const int hud[] = {0,1,2,3,8};
    size_t i;
    for (i = 0; i < sizeof(hud)/sizeof(hud[0]); i++) {
        LazyAsset *e = lookup(a, &a->ui.panel_data[hud[i]]);
        if (e && acquire_asset(a,e)) e->prepared = 1;
    }
#if defined(RICH4_PLATFORM_MIYOO)
    /* Device trace: Panel 63/78/79/80 and Data 553 dominate first-use stalls.
     * These are already bound to their verified archive handles above. Warm
     * immutable bytes behind the existing loading screen, within the same
     * 4 MiB LRU budget; do not create an animation state or pin the resources. */
    {
        static const int event_panels[] = {63,78,79,80};
        for (i = 0; i < sizeof(event_panels)/sizeof(event_panels[0]); i++) {
            LazyAsset *e = lookup(a, &a->ui.panel_data[event_panels[i]]);
            if (e && acquire_asset(a,e)) release_asset(a,e,1);
        }
        {
            LazyAsset *e = lookup(a, &a->tool.tool_effect_data[4]); /* Data 553 */
            if (e && acquire_asset(a,e)) release_asset(a,e,1);
        }
    }
#endif
    for (character = 0; character < GAME_JUMP_CHARACTER_COUNT; character++)
        if (a->active_characters & (1u << character)) {
            LazyAsset *e = lookup(a, &a->character.first_landing_data[character]);
            if (e && acquire_asset(a,e)) e->prepared = 1;
        }
    (void)game_asset_ready(a, &a->intro.has_jump_intro_scene);
    (void)game_asset_ready(a, &a->intro.has_jump_intro_window_graph);
    for (phase = 0; phase < 2; phase++)
        for (character = 0; character < GAME_JUMP_CHARACTER_COUNT; character++)
            if (a->active_characters & (1u << character))
                (void)game_asset_ready(a, &a->character.has_jump_intro_actor_graph[phase][character]);
}

void game_lazy_assets_begin_frame(GamePreviewAssets *a)
{
    size_t i;
    if (a->lazy == NULL) return;
    a->lazy->frame++;
    for (i=0;i<a->lazy->count;i++) {
        LazyAsset *e=&a->lazy->entries[i];
        if (!e->prepared && a->lazy->frame-e->used_frame > 1) release_asset(a,e,1);
    }
}

void game_lazy_assets_clear_recent(GamePreviewAssets *a)
{
    size_t i;
    if (a->lazy == NULL) return;
    for (i=0;i<WARM_MAX;i++) rich4_resource_store_release(a->resource_store,&a->lazy->warm[i].handle);
    a->lazy->warm_bytes = 0;
}

void game_lazy_assets_free(GamePreviewAssets *a)
{
    size_t i;
    if (a->lazy == NULL) return;
    for (i=0;i<a->lazy->count;i++) release_asset(a,&a->lazy->entries[i],0);
    game_lazy_assets_clear_recent(a);
    free(a->lazy); a->lazy=NULL;
}

Rich4ResourceHandle game_lazy_asset_handle(const GamePreviewAssets *a,
                                          const unsigned char *data)
{
    size_t i;
    Rich4ResourceHandle none;
    memset(&none,0,sizeof(none));
    if (a != NULL && a->lazy != NULL)
        for(i=0;i<a->lazy->count;i++)
            if(a->lazy->entries[i].handle.data==data) return a->lazy->entries[i].handle;
    return none;
}
