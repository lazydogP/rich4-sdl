#include "platform/sdl1/runtime.h"
#include "platform/sdl1/test_adapter/diff_adapter.h"

#include <SDL/SDL.h>
#include <stdlib.h>

#include "view/map_renderer.h"
#include "platform/sdl1/render/pixel_rows.h"
#include "platform/sdl1/runtime_trace.h"

static SDL_Surface *rich4_sdl1_display;
static Uint32 rich4_sdl1_rgb565_present_lut[65536];
static int rich4_sdl1_deterministic_ticks;
static Uint32 rich4_sdl1_deterministic_now;
static Uint32 rich4_sdl1_frame_started, rich4_sdl1_wakeup;
static int rich4_sdl1_debug_capture_pending;

int rich4_sdl1_request_debug_capture(void)
{
    const char *path = getenv("RICH4_UI_QA_BMP");
    if (path == NULL || path[0] == '\0') return 0;
    rich4_sdl1_debug_capture_pending = 1;
    return 1;
}

static void rich4_sdl1_debug_capture_if_requested(void)
{
    const char *path;
    if (!rich4_sdl1_debug_capture_pending) return;
    rich4_sdl1_debug_capture_pending = 0;
    path = getenv("RICH4_UI_QA_BMP");
    if (path != NULL && path[0] != '\0' && rich4_sdl1_display != NULL)
        (void)SDL_SaveBMP(rich4_sdl1_display, path);
}

static void rich4_sdl1_build_present_lut(SDL_PixelFormat *format)
{
    unsigned int pixel;
    for (pixel = 0; pixel < 65536; pixel++) {
        uint32_t rgb = rich4_rgb565_to_rgb888_truncated((uint16_t)pixel);
        rich4_sdl1_rgb565_present_lut[pixel] = SDL_MapRGB(
            format,
            (Uint8)((rgb >> 16) & 0xffu),
            (Uint8)((rgb >> 8) & 0xffu),
            (Uint8)(rgb & 0xffu));
    }
}

SDL_Surface *rich4_sdl1_create_window(int width,
                                      int height,
                                      int bits_per_pixel,
                                      const char *title)
{
    SDL_Surface *screen;
    rich4_sdl1_display = SDL_SetVideoMode(
        width, height, bits_per_pixel == 16 ? 32 : bits_per_pixel,
#if defined(RICH4_PLATFORM_MIYOO)
        SDL_HWSURFACE | SDL_DOUBLEBUF);
#else
        SDL_SWSURFACE);
#endif
    if (rich4_sdl1_display == NULL) return NULL;
    if (title != NULL) SDL_WM_SetCaption(title, title);
    if (bits_per_pixel != 16) return rich4_sdl1_display;
    if (rich4_sdl1_display->format == NULL ||
        rich4_sdl1_display->format->BytesPerPixel != 4) {
        SDL_SetError("32-bit presentation surface unavailable");
        return NULL;
    }
    screen = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, 16,
                                  0xf800, 0x07e0, 0x001f, 0);
    if (screen == NULL) return NULL;
    rich4_sdl1_build_present_lut(rich4_sdl1_display->format);
    return screen;
}

uint32_t rich4_sdl1_ticks(void)
{
    if (r4diff_active()) return r4diff_ticks();
    return rich4_sdl1_deterministic_ticks
        ? (uint32_t)rich4_sdl1_deterministic_now
        : (uint32_t)SDL_GetTicks();
}

void rich4_sdl1_set_deterministic_ticks(int enabled)
{
    rich4_sdl1_deterministic_ticks = enabled != 0;
    rich4_sdl1_deterministic_now = 0;
}

int rich4_sdl1_present(SDL_Surface *screen)
{
    int y, width, direct_rgb888, result;
    int rotate = 0;
    unsigned int started;
#if defined(RICH4_PLATFORM_MIYOO)
    /* Mini's panel is mounted upside down, as verified by the borrowed
     * kit/platform/miyoo_panel.c. Some OS SDL builds already rotate it. */
    const char *rotate_env = getenv("RICH4_MIYOO_ROTATE");
    rotate = rotate_env == NULL || rotate_env[0] != '0';
#endif
    if (screen == NULL || rich4_sdl1_display == NULL) return 0;
    r4diff_capture(screen);
    if (screen == rich4_sdl1_display) {
        started = runtime_profile_now();
        rich4_sdl1_debug_capture_if_requested();
        runtime_profile_detail(PROFILE_PRESENT_CAPTURE, started);
        started = runtime_profile_now();
        result = SDL_Flip(screen) == 0;
        runtime_profile_detail(PROFILE_PRESENT_FLIP, started);
        return result;
    }
    if (screen->format == NULL || screen->format->BitsPerPixel != 16 ||
        rich4_sdl1_display->format == NULL ||
        rich4_sdl1_display->format->BytesPerPixel != 4) return 0;
    started = runtime_profile_now();
    if (SDL_MUSTLOCK(screen) && SDL_LockSurface(screen) != 0) return 0;
    if (SDL_MUSTLOCK(rich4_sdl1_display) &&
        SDL_LockSurface(rich4_sdl1_display) != 0) {
        if (SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
        return 0;
    }
    runtime_profile_detail(PROFILE_PRESENT_LOCK, started);
    started = runtime_profile_now();
    width = screen->w < rich4_sdl1_display->w ? screen->w : rich4_sdl1_display->w;
    direct_rgb888 = rich4_sdl1_display->format->Rmask == 0x00ff0000u &&
        rich4_sdl1_display->format->Gmask == 0x0000ff00u &&
        rich4_sdl1_display->format->Bmask == 0x000000ffu &&
        (rich4_sdl1_display->format->Amask == 0 ||
         rich4_sdl1_display->format->Amask == 0xff000000u);
    for (y = 0; y < screen->h && y < rich4_sdl1_display->h; y++) {
        int display_y = rotate ? rich4_sdl1_display->h - 1 - y : y;
        const uint16_t *src = (const uint16_t *)(
            (const unsigned char *)screen->pixels + y * screen->pitch);
        Uint32 *dst = (Uint32 *)(
            (unsigned char *)rich4_sdl1_display->pixels +
            display_y * rich4_sdl1_display->pitch);
        int x;
        if (rotate) dst += rich4_sdl1_display->w - width;
        if (direct_rgb888)
            rich4_present_rgb888_row(dst, src, width, rotate,
                                     rich4_sdl1_display->format->Amask);
        else if (rotate)
            for (x = 0; x < width; x++) dst[width - 1 - x] = rich4_sdl1_rgb565_present_lut[src[x]];
        else
            for (x = 0; x < width; x++) dst[x] = rich4_sdl1_rgb565_present_lut[src[x]];
    }
    runtime_profile_detail(PROFILE_PRESENT_CONVERT, started);
    started = runtime_profile_now();
    if (SDL_MUSTLOCK(rich4_sdl1_display))
        SDL_UnlockSurface(rich4_sdl1_display);
    if (SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
    runtime_profile_detail(PROFILE_PRESENT_UNLOCK, started);
    started = runtime_profile_now();
    rich4_sdl1_debug_capture_if_requested();
    runtime_profile_detail(PROFILE_PRESENT_CAPTURE, started);
    started = runtime_profile_now();
    result = SDL_Flip(rich4_sdl1_display) == 0;
    runtime_profile_detail(PROFILE_PRESENT_FLIP, started);
    return result;
}

void rich4_sdl1_destroy_window(SDL_Surface *screen)
{
    if (screen != NULL && screen != rich4_sdl1_display)
        SDL_FreeSurface(screen);
    rich4_sdl1_display = NULL;
}

void rich4_sdl1_delay(uint32_t delay_ms)
{
    if (r4diff_active()) {r4diff_delay(delay_ms);return;}
    if (rich4_sdl1_deterministic_ticks) {
        rich4_sdl1_deterministic_now += (Uint32)delay_ms;
        return;
    }
    SDL_Delay((Uint32)delay_ms);
}

static uint32_t rich4_sdl1_game_frame_started;

uint32_t rich4_sdl1_frame_start_ticks(void)
{
    return rich4_sdl1_game_frame_started;
}

void rich4_sdl1_begin_frame(void)
{
    if (r4diff_active()) r4diff_begin();
    rich4_sdl1_frame_started = SDL_GetTicks();
    rich4_sdl1_game_frame_started = rich4_sdl1_ticks();
    /* 0x4020FA: cursor/input timer quantum, not the game's animation rate. */
    rich4_sdl1_wakeup = rich4_sdl1_frame_started + 20u;
}

void rich4_sdl1_wake_at(uint32_t deadline)
{
    if (r4diff_active()) {r4diff_wake_at(deadline);return;}
    if (rich4_sdl1_deterministic_ticks) return;
    if ((int32_t)(deadline-rich4_sdl1_wakeup) < 0)
        rich4_sdl1_wakeup = deadline;
}

void rich4_sdl1_finish_frame(void)
{
    int32_t remaining;
    if (r4diff_active()) return;
    if (rich4_sdl1_deterministic_ticks) {
        /* Keep existing pixel/replay fixtures on their 16 ms logical clock.
         * This test clock does not measure or emulate interactive waiting. */
        rich4_sdl1_deterministic_now += 16u;
        return;
    }
    remaining = (int32_t)(rich4_sdl1_wakeup-SDL_GetTicks());
    if (remaining > 0) SDL_Delay((Uint32)remaining);
}
