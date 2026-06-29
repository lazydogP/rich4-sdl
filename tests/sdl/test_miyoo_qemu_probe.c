/* QEMU-only probe of the production Mini adapter and production Big5 atlas.
 * The text below is a diagnostic specimen, not a new game UI. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <SDL/SDL.h>
#ifdef _WIN32
#undef main
#endif
#include "platform/sdl1/miyoo_input.h"
#include "platform/sdl1/input_adapter.h"
#include "platform/sdl1/text/font_atlas.h"
#include "platform/sdl1/text/text_backend_atlas.h"

int main(int argc, char **argv)
{
    const char *sample = "\xa4\x6a\xb4\x49\xaf\xce\x34\x20"
        "\xb9\x43\xc0\xb8\xb3\x5d\xa9\x77\x20"
        "\xa6\xdb\xb0\xca\xa6\x73\xc0\xc9\x20"
        "\xa4\x48\xaa\xab\xb8\xea\xae\xc6";
    const SDLKey native[] = {SDLK_LSHIFT, SDLK_LALT, SDLK_e, SDLK_t,
        SDLK_TAB, SDLK_BACKSPACE, SDLK_RCTRL, SDLK_RETURN, SDLK_ESCAPE,
        SDLK_UP, SDLK_DOWN, SDLK_LEFT, SDLK_RIGHT};
    const Rich4AppKey normalized[] = {RICH4_APP_KEY_D, RICH4_APP_KEY_RETURN,
        RICH4_APP_KEY_PAGEUP, RICH4_APP_KEY_PAGEDOWN, RICH4_APP_KEY_C,
        RICH4_APP_KEY_LETTER_W, RICH4_APP_KEY_TAB, RICH4_APP_KEY_RETURN,
        RICH4_APP_KEY_ESCAPE, RICH4_APP_KEY_UP, RICH4_APP_KEY_DOWN,
        RICH4_APP_KEY_LEFT, RICH4_APP_KEY_RIGHT};
    Rich4FontAtlas atlas = {0};
    Rich4TextBackend backend = {0};
    Rich4TextStyle style = {16, 0, 1, 0, 0xffff, 0};
    Rich4TextTarget target;
    SDL_Surface *surface;
    unsigned int i;
    int up, glyphs = 0, visible = 0, x;
    const unsigned char *p = (const unsigned char *)sample;
    assert(argc == 2);
    setvbuf(stdout, NULL, _IOLBF, 0);
    for (up = 0; up <= 1; up++) {
        for (i = 0; i < sizeof(native) / sizeof(native[0]); i++) {
            SDL_Event event;
            Rich4AppCommand command;
            memset(&event, 0, sizeof(event));
            event.type = up ? SDL_KEYUP : SDL_KEYDOWN;
            event.key.keysym.sym = native[i];
            event.key.keysym.mod = KMOD_CTRL | KMOD_ALT | KMOD_SHIFT;
            rich4_sdl1_miyoo_adapt_event(&event, 123, 456);
            assert(rich4_sdl1_key_command(&command,
                up ? RICH4_APP_COMMAND_KEY_UP : RICH4_APP_COMMAND_KEY_DOWN,
                event.key.keysym.sym, event.key.keysym.mod));
            assert(command.key == normalized[i] && command.modifiers == 0);
        }
        for (i = 0; i < 2; i++) {
            SDL_Event event;
            memset(&event, 0, sizeof(event));
            event.type = up ? SDL_KEYUP : SDL_KEYDOWN;
            event.key.keysym.sym = i ? SDLK_LCTRL : SDLK_SPACE;
            rich4_sdl1_miyoo_adapt_event(&event, 123, 456);
            assert(event.type == (up ? SDL_MOUSEBUTTONUP : SDL_MOUSEBUTTONDOWN));
            assert(event.button.button == (i ? SDL_BUTTON_RIGHT : SDL_BUTTON_LEFT));
            assert(event.button.x == 123 && event.button.y == 456);
        }
    }
    puts("QA_MAPPING_PASS physical_buttons=15 press_release_cases=30");
    assert(rich4_font_atlas_load_file(&atlas, "assets/fonts/rich4_font.r4atlas"));
    while (*p) {
        uint16_t code = *p++;
        const Rich4FontGlyph *glyph;
        if (code >= 0x80) code = (uint16_t)((code << 8) | *p++);
        glyph = rich4_font_atlas_find(&atlas, code);
        assert(glyph != NULL);
        if (code >= 0x80) { assert(glyph->width && glyph->height); glyphs++; }
    }
    assert(rich4_text_backend_atlas_init(&backend, &atlas));
    assert(SDL_Init(0) == 0);
    surface = SDL_CreateRGBSurface(SDL_SWSURFACE, 640, 80, 16,
                                   0xf800, 0x07e0, 0x001f, 0);
    assert(surface != NULL);
    SDL_FillRect(surface, NULL, 0);
    target.pixels = surface->pixels;
    target.pitch_bytes = surface->pitch;
    target.width = surface->w;
    target.height = surface->h;
    assert(backend.render(&backend, &target, 20, 20, sample, &style));
    for (x = 0; x < surface->pitch * surface->h / 2; x++)
        if (target.pixels[x]) visible++;
    assert(visible > 100);
    assert(SDL_SaveBMP(surface, argv[1]) == 0);
    printf("QA_CHINESE_PASS chinese_glyphs=%d rendered_pixels=%d atlas_glyphs=%u\n",
           glyphs, visible, (unsigned int)atlas.glyph_count);
    SDL_FreeSurface(surface);
    backend.shutdown(&backend);
    rich4_font_atlas_close(&atlas);
    SDL_Quit();
    return 0;
}
