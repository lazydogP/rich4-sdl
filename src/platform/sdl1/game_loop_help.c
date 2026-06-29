#include "platform/sdl1/game_loop_help_data.h"
#include <stdio.h>
#include <string.h>

#include "app/app_command.h"
#include "app/config_types.h"
#include "app/input_flow.h"
#include "platform/sdl1/input_adapter.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/app_panel_types.h"
#include "resource/mkf.h"
#include "platform/sdl1/runtime_bootstrap.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/runtime_render_api.h"
#include "resource/spr_smp.h"
#include "view/map_renderer.h"

int game_load_help_assets(ResourceStore *store,
                            const char *data_dir,
                            HelpAssets *assets)
{
    static const char *const names[] = {
        "Help.mkf", "HELP.MKF", "help.mkf"
    };
    char path[512];
    Rich4MkfError err;

    memset(assets, 0, sizeof(*assets));
    assets->store = store;
    assets->mkf = game_open_named_mkf_from_data_dir(
        store, data_dir, names, sizeof(names) / sizeof(names[0]),
        path, sizeof(path));
    if (assets->mkf == NULL || rich4_mkf_count(assets->mkf) < 100)
        return 0;
    err = rich4_resource_store_borrow(
        store, assets->mkf, 0, &assets->ui_resource, NULL);
    if (err != RICH4_MKF_OK)
        return 0;
    assets->ui_data = assets->ui_resource.data;
    assets->ui_size = assets->ui_resource.size;
    if (!rich4_spr_smp_probe(
            assets->ui_data, assets->ui_size, &assets->ui_spr) ||
        assets->ui_spr.num_chunks < 12)
        return 0;
    assets->available = 1;
    printf("opened Help MKF: %s\n", path);
    return 1;
}

static int help_draw_chunk(SDL_Surface *screen,
                           const HelpAssets *assets,
                           size_t chunk_index,
                           int x,
                           int y)
{
    Rich4SprSmpChunk chunk;
    if (screen == NULL || assets == NULL || !assets->available ||
        rich4_spr_smp_get_chunk(
            assets->ui_data, assets->ui_size, &assets->ui_spr,
            chunk_index, &chunk) != RICH4_SPR_SMP_OK)
        return 0;
    return draw_chunk_topleft(screen, assets->ui_data, &chunk, x, y, 0);
}

static int help_selected_text(const HelpAssets *assets,
                              const HelpPanelState *panel,
                              Rich4ResourceHandle *out)
{
    int resource;
    if (assets == NULL || panel == NULL || out == NULL ||
        !assets->available || panel->category < 0 ||
        panel->category >= HELP_CATEGORY_COUNT ||
        panel->selected_item < 0 ||
        panel->selected_item >=
            help_category_item_count[panel->category])
        return 0;
    resource = help_category_item_offset[panel->category] +
               panel->selected_item + 1;
    return rich4_resource_store_borrow(
               assets->store, assets->mkf, (size_t)resource,
               out, NULL) == RICH4_MKF_OK;
}

static size_t help_bounded_strlen(const unsigned char *text, size_t size)
{
    size_t length = 0;
    while (length < size && text[length] != 0)
        length++;
    return length;
}

static int help_page_starts(const unsigned char *data,
                            size_t size,
                            size_t *starts,
                            int capacity)
{
    size_t offset = 0;
    int lines = 0;
    int pages = 0;
    if (data == NULL || size == 0 || starts == NULL || capacity <= 0)
        return 0;
    starts[pages++] = 0;
    while (offset < size && data[offset] != 0) {
        size_t length;
        if (data[offset] == '@') {
            offset++;
            if (offset < size && data[offset] == 0)
                offset++;
            lines = 0;
            if (offset < size && data[offset] != 0 && pages < capacity)
                starts[pages++] = offset;
            continue;
        }
        length = help_bounded_strlen(data + offset, size - offset);
        if (length >= size - offset)
            break;
        offset += length + 1;
        lines++;
        if (lines == HELP_BODY_LINES && offset < size &&
            data[offset] != 0) {
            if (data[offset] == '@') {
                offset++;
                if (offset < size && data[offset] == 0)
                    offset++;
            }
            lines = 0;
            if (offset < size && data[offset] != 0 && pages < capacity)
                starts[pages++] = offset;
        }
    }
    return pages;
}

static int help_panel_page_count(const HelpAssets *assets,
                                 const HelpPanelState *panel)
{
    Rich4ResourceHandle text;
    size_t starts[64];
    if (!help_selected_text(assets, panel, &text))
        return 0;
    return help_page_starts(text.data, text.size, starts,
                            (int)(sizeof(starts) / sizeof(starts[0])));
}

static int help_control_at(int x, int y)
{
    static const int rects[6][4] = {
        {22, 38, 91, 361},
        {104, 78, 196, 361},
        {170, 43, 192, 58},
        {170, 59, 192, 74},
        {322, 48, 345, 80},
        {343, 48, 366, 80}
    };
    int i;
    x -= HELP_LEFT;
    y -= HELP_TOP;
    for (i = 0; i < 6; i++) {
        if (x >= rects[i][0] && y >= rects[i][1] &&
            x < rects[i][2] && y < rects[i][3])
            return i;
    }
    return -1;
}

static int help_category_at(int x, int y)
{
    int category;
    x -= HELP_LEFT;
    y -= HELP_TOP;
    for (category = 0; category < HELP_CATEGORY_COUNT; category++) {
        if (x >= 26 && x < 92 &&
            y >= 36 * category + 58 &&
            y < 36 * category + 91)
            return category;
    }
    return -1;
}

static int help_visible_item_at(const HelpPanelState *panel, int x, int y)
{
    int row;
    int visible;
    int count;
    if (panel == NULL)
        return -1;
    x -= HELP_LEFT;
    y -= HELP_TOP;
    count = help_category_item_count[panel->category];
    visible = count - panel->scroll[panel->category];
    if (visible > HELP_VISIBLE_ITEMS)
        visible = HELP_VISIBLE_ITEMS;
    for (row = 0; row < visible; row++) {
        if (x >= 108 && x < 193 &&
            y >= 34 * row + 78 && y < 34 * row + 111)
            return panel->scroll[panel->category] + row;
    }
    return -1;
}

int game_open_help_panel(HelpPanelState *panel,
                           const HelpAssets *assets,
                           SDL_Surface *capture_source)
{
    SDL_Surface *saved;
    if (panel == NULL || assets == NULL || !assets->available ||
        capture_source == NULL)
        return 0;
    saved = SDL_CreateRGBSurface(
        SDL_SWSURFACE, capture_source->w, capture_source->h,
        capture_source->format->BitsPerPixel,
        capture_source->format->Rmask, capture_source->format->Gmask,
        capture_source->format->Bmask, capture_source->format->Amask);
    if (saved == NULL ||
        SDL_BlitSurface(capture_source, NULL, saved, NULL) != 0) {
        if (saved != NULL)
            SDL_FreeSurface(saved);
        return 0;
    }
    if (panel->saved_background != NULL)
        SDL_FreeSurface(panel->saved_background);
    panel->saved_background = saved;
    panel->active = 1;
    panel->button_pressed = 0;
    panel->category = 0;
    panel->selected_item = 0;
    panel->page = 0;
    panel->pressed_control = -1;
    return 1;
}

void game_close_help_panel(HelpPanelState *panel)
{
    if (panel == NULL)
        return;
    panel->active = 0;
    panel->button_pressed = 0;
    panel->pressed_control = -1;
    if (panel->saved_background != NULL) {
        SDL_FreeSurface(panel->saved_background);
        panel->saved_background = NULL;
    }
}

void game_help_panel_mouse_down(HelpPanelState *panel,
                                  const HelpAssets *assets,
                                  Rich4Audio *audio,
                                  int x,
                                  int y)
{
    int control;
    int choice;
    int count;
    int pages;
    if (panel == NULL || !panel->active)
        return;
    control = help_control_at(x, y);
    panel->pressed_control = -1;
    if (control < 0)
        return;
    /* sub_44E40B plays Effect.mkf[1] for every one of the six hit boxes. */
    rich4_audio_play_ok(audio);
    switch (control) {
    case 0:
        choice = help_category_at(x, y);
        if (choice >= 0) {
            panel->category = choice;
            panel->selected_item = 0;
            panel->page = 0;
        }
        break;
    case 1:
        choice = help_visible_item_at(panel, x, y);
        if (choice >= 0) {
            panel->selected_item = choice;
            panel->page = 0;
        }
        break;
    case 2:
    case 3:
        count = help_category_item_count[panel->category];
        if (count > HELP_VISIBLE_ITEMS)
            panel->pressed_control = control;
        break;
    case 4:
    case 5:
        pages = help_panel_page_count(assets, panel);
        if (pages > 1)
            panel->pressed_control = control;
        break;
    default:
        break;
    }
}

void game_help_panel_mouse_up(HelpPanelState *panel,
                                const HelpAssets *assets,
                                int x,
                                int y)
{
    int control;
    int count;
    int maximum;
    int delta;
    int pages;
    if (panel == NULL || !panel->active)
        return;
    control = help_control_at(x, y);
    panel->pressed_control = -1;
    count = help_category_item_count[panel->category];
    switch (control) {
    case 2:
        delta = panel->scroll[panel->category];
        if (delta > HELP_VISIBLE_ITEMS)
            delta = HELP_VISIBLE_ITEMS;
        panel->scroll[panel->category] -= delta;
        panel->selected_item -= delta;
        break;
    case 3:
        maximum = count - HELP_VISIBLE_ITEMS;
        if (maximum < 0)
            maximum = 0;
        delta = maximum - panel->scroll[panel->category];
        if (delta > HELP_VISIBLE_ITEMS)
            delta = HELP_VISIBLE_ITEMS;
        panel->scroll[panel->category] += delta;
        panel->selected_item += delta;
        break;
    case 4:
        pages = help_panel_page_count(assets, panel);
        if (pages > 1 && panel->page > 0)
            panel->page--;
        break;
    case 5:
        pages = help_panel_page_count(assets, panel);
        if (pages > 1 && panel->page + 1 < pages)
            panel->page++;
        break;
    default:
        break;
    }
}

static int help_hotkey_matches(const Rich4Key *hotkey,
                               Rich4AppKey key, int key_modifiers)
{
    int control;
    if (hotkey == NULL || hotkey->key != rich4_sdl1_original_hotkey(key))
        return 0;
    control = (key_modifiers & (KMOD_LCTRL | KMOD_RCTRL)) != 0;
    return hotkey->mod == 0x11 ? control : !control;
}

void game_help_panel_key_down(HelpPanelState *panel,
                                const HelpAssets *assets,
                                const Rich4Config *config,
                                Rich4Audio *audio,
                                Rich4AppKey key, int key_modifiers)
{
    int pages;
    if (panel == NULL || !panel->active || config == NULL)
        return;
    pages = help_panel_page_count(assets, panel);
    if (pages <= 1)
        return;
    if (help_hotkey_matches(&config->hotkeys[25], key, key_modifiers)) {
        rich4_audio_play_ok(audio);
        if (panel->page > 0)
            panel->page--;
    } else if (help_hotkey_matches(&config->hotkeys[26], key, key_modifiers)) {
        rich4_audio_play_ok(audio);
        if (panel->page + 1 < pages)
            panel->page++;
    }
}

void game_draw_help_panel(SDL_Surface *screen,
                            const HelpAssets *assets,
                            const HelpPanelState *panel)
{
    Rich4ResourceHandle text;
    size_t starts[64];
    size_t offset;
    int pages;
    int row;
    int count;
    int visible;
    uint16_t dark = rich4_character_rgb565_color(0x00101010u);
    if (screen == NULL || assets == NULL || panel == NULL ||
        !panel->active || !assets->available)
        return;
    if (panel->saved_background != NULL)
        SDL_BlitSurface(panel->saved_background, NULL, screen, NULL);
    help_draw_chunk(screen, assets, 0, HELP_LEFT, HELP_TOP);
    for (row = 0; row < HELP_CATEGORY_COUNT; row++)
        draw_big5_text_with_spacing(
            screen, HELP_LEFT + 59, HELP_TOP + 36 * row + 73,
            help_category_names[row], 12, dark, dark, 0, 1, 2);
    help_draw_chunk(screen, assets, 1, HELP_LEFT + 26,
                    HELP_TOP + 36 * panel->category + 58);
    draw_big5_text_with_spacing(
        screen, HELP_LEFT + 59, HELP_TOP + 36 * panel->category + 73,
        help_category_names[panel->category],
        12, dark, dark, 0, 1, 2);
    draw_big5_text_with_spacing(
        screen, HELP_LEFT + 140, HELP_TOP + 57,
        help_category_names[panel->category],
        12, dark, dark, 0, 1, 2);

    count = help_category_item_count[panel->category];
    visible = count - panel->scroll[panel->category];
    if (visible > HELP_VISIBLE_ITEMS)
        visible = HELP_VISIBLE_ITEMS;
    if (count > HELP_VISIBLE_ITEMS) {
        help_draw_chunk(screen, assets,
                        panel->pressed_control == 2 ? 6 : 4,
                        HELP_LEFT + 170, HELP_TOP + 43);
        help_draw_chunk(screen, assets,
                        panel->pressed_control == 3 ? 7 : 5,
                        HELP_LEFT + 170, HELP_TOP + 59);
    }
    for (row = 0; row < visible; row++) {
        int item = panel->scroll[panel->category] + row;
        int label = help_item_label_index(panel->category, item);
        help_draw_chunk(screen, assets,
                        item == panel->selected_item ? 3 : 2,
                        HELP_LEFT + 108, HELP_TOP + 34 * row + 78);
        draw_big5_text_with_spacing(
            screen, HELP_LEFT + 150, HELP_TOP + 34 * row + 94,
            help_item_names[label], 15, dark, dark, 2, 0, 2);
    }
    draw_big5_text_with_spacing(
        screen, HELP_LEFT + 270, HELP_TOP + 63,
        help_item_names[help_item_label_index(panel->category,
                                              panel->selected_item)],
        15, dark, dark, 2, 0, 2);

    if (!help_selected_text(assets, panel, &text))
        return;
    pages = help_page_starts(
        text.data, text.size, starts,
        (int)(sizeof(starts) / sizeof(starts[0])));
    if (pages <= 0)
        return;
    if (panel->page >= pages)
        offset = starts[pages - 1];
    else
        offset = starts[panel->page];
    for (row = 0; row < HELP_BODY_LINES && offset < text.size; row++) {
        size_t length;
        char line[512];
        if (text.data[offset] == 0 || text.data[offset] == '@')
            break;
        length = help_bounded_strlen(text.data + offset, text.size - offset);
        if (length >= text.size - offset)
            break;
        if (length >= sizeof(line))
            length = sizeof(line) - 1;
        memcpy(line, text.data + offset, length);
        line[length] = '\0';
        draw_big5_text_with_spacing(
            screen, HELP_LEFT + 232, HELP_TOP + 18 * row + 90,
            line, 12, dark, dark, 0, 1, 0);
        offset += help_bounded_strlen(
            text.data + offset, text.size - offset) + 1;
    }
    if (pages > 1) {
        help_draw_chunk(screen, assets,
                        panel->pressed_control == 4 ? 10 : 8,
                        HELP_LEFT + 322, HELP_TOP + 48);
        help_draw_chunk(screen, assets,
                        panel->pressed_control == 5 ? 11 : 9,
                        HELP_LEFT + 343, HELP_TOP + 48);
    }
}
