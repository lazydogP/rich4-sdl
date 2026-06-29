/* A10.2: production text entry point.  Runtime rendering selects the profile
 * backend once; this adapter only translates SDL_Surface to the backend's
 * borrowed RGB565 target and keeps the legacy resource-text prefix handling.
 */

#include <stdio.h>
#include <string.h>

#include <SDL/SDL.h>

#include "platform/platform_profile.h"
#include "platform/sdl1/runtime_trace.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/text/font_atlas.h"
#include "platform/sdl1/text/runtime_text.h"
#include "platform/sdl1/text/text_backend_selector.h"
#include "platform/sdl1/text_rules.h"

typedef struct Rich4RuntimeText {
    Rich4TextBackend backend;
    Rich4FontAtlas atlas;
    int initialized;
    int available;
} Rich4RuntimeText;

static Rich4RuntimeText g_runtime_text;
static char g_runtime_font_path[512];

void rich4_sdl_text_set_font_path(const char *font_path)
{
    if (font_path == NULL) {
        g_runtime_font_path[0] = '\0';
        return;
    }
    snprintf(g_runtime_font_path, sizeof(g_runtime_font_path), "%s",
             font_path);
}

static int rich4_runtime_text_init(void)
{
    if (g_runtime_text.initialized) {
        return g_runtime_text.available;
    }
    memset(&g_runtime_text, 0, sizeof(g_runtime_text));
    g_runtime_text.initialized = 1;
    rich4_text_backend_set_font_path(g_runtime_font_path);
#if RICH4_TEXT_BACKEND == RICH4_TEXT_BACKEND_ATLAS
    if (rich4_text_backend_uses_atlas() && !rich4_font_atlas_load_file(&g_runtime_text.atlas,
                                    RICH4_FONT_ATLAS_PATH)) {
        return 0;
    }
    g_runtime_text.available = rich4_text_backend_init_for_profile(
        &g_runtime_text.backend, &g_runtime_text.atlas);
#else
    g_runtime_text.available = rich4_text_backend_init_for_profile(
        &g_runtime_text.backend, NULL);
#endif
    fprintf(stderr, "TEXT_BACKEND name=%s available=%d\n",
            rich4_text_backend_uses_atlas() ? "atlas" : "ttf",
            g_runtime_text.available);
    return g_runtime_text.available;
}

void rich4_sdl_text_shutdown(void)
{
    if (!g_runtime_text.initialized) {
        return;
    }
    rich4_text_backend_shutdown(&g_runtime_text.backend);
#if RICH4_TEXT_BACKEND == RICH4_TEXT_BACKEND_ATLAS
    rich4_font_atlas_close(&g_runtime_text.atlas);
#endif
    memset(&g_runtime_text, 0, sizeof(g_runtime_text));
}

void draw_big5_text_with_spacing(SDL_Surface *screen,
                                 int x,
                                 int y,
                                 const char *text,
                                 int height,
                                 uint16_t fg,
                                 uint16_t shadow,
                                 int style,
                                 int spacing,
                                 int anchor)
{
    Rich4TextTarget target;
    Rich4TextStyle text_style;
    int rendered = 0;
    unsigned int started;

    if (screen == NULL || text == NULL || text[0] == '\0') {
        return;
    }
    if (text[0] == '#') {
        if (strlen(text) < 5) {
            return;
        }
        text += 5;
        if (text[0] == '\0') {
            return;
        }
    }
    started = runtime_profile_now();
    if (rich4_runtime_text_init()) {
        target.pixels = (uint16_t *)screen->pixels;
        target.pitch_bytes = screen->pitch;
        target.width = screen->w;
        target.height = screen->h;
        text_style.height = height;
        text_style.style = style;
        text_style.spacing = spacing;
        text_style.mode = anchor;
        text_style.fg = fg;
        text_style.shadow = shadow;
        if (SDL_MUSTLOCK(screen)) {
            SDL_LockSurface(screen);
        }
        rendered = rich4_text_backend_render(&g_runtime_text.backend,
                                             &target, x, y, text,
                                             &text_style);
        if (SDL_MUSTLOCK(screen)) {
            SDL_UnlockSurface(screen);
        }
    }
    if (!rendered) {
        /* The bitmap fallback is 5x7 with a six-pixel advance. Honor the
         * same anchor used by the primary backend, including save dates. */
        int width = (int)strlen(text) * 6;
        if (width > 0) width--;
        rich4_text_apply_anchor(anchor, width, 7, &x, &y);
        draw_text(screen, x, y, text, 1, fg);
    }
    runtime_profile_detail(PROFILE_TEXT_RENDER, started);
}

void draw_big5_text(SDL_Surface *screen,
                    int x,
                    int y,
                    const char *text,
                    int height,
                    uint16_t fg,
                    uint16_t shadow,
                    int style,
                    int anchor)
{
    draw_big5_text_with_spacing(screen, x, y, text, height, fg, shadow,
                                style, 1, anchor);
}
