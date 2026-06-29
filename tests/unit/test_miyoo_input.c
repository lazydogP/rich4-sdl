#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "platform/sdl1/miyoo_input.h"

int main(void)
{
    const SDLKey physical[] = {SDLK_LSHIFT, SDLK_LALT, SDLK_e, SDLK_t,
        SDLK_TAB, SDLK_BACKSPACE, SDLK_RCTRL, SDLK_RETURN, SDLK_ESCAPE,
        SDLK_UP, SDLK_DOWN, SDLK_LEFT, SDLK_RIGHT};
    const SDLKey mapped[] = {SDLK_d, SDLK_RETURN, SDLK_PAGEUP, SDLK_PAGEDOWN,
        SDLK_c, SDLK_w, SDLK_TAB, SDLK_RETURN, SDLK_ESCAPE,
        SDLK_UP, SDLK_DOWN, SDLK_LEFT, SDLK_RIGHT};
    SDL_Event event;
    unsigned int i;
    int up;
    for (up = 0; up <= 1; ++up) {
        for (i = 0; i < sizeof(physical) / sizeof(physical[0]); ++i) {
            memset(&event, 0, sizeof(event));
            event.type = up ? SDL_KEYUP : SDL_KEYDOWN;
            event.key.keysym.sym = physical[i];
            event.key.keysym.mod = KMOD_CTRL | KMOD_ALT | KMOD_SHIFT;
            rich4_sdl1_miyoo_adapt_event(&event, 123, 456);
            assert(event.type == (up ? SDL_KEYUP : SDL_KEYDOWN));
            assert(event.key.keysym.sym == mapped[i]);
            assert(event.key.keysym.mod == KMOD_NONE);
        }
        for (i = 0; i < 2; ++i) {
            event.type = up ? SDL_KEYUP : SDL_KEYDOWN;
            event.key.keysym.sym = i ? SDLK_LCTRL : SDLK_SPACE;
            rich4_sdl1_miyoo_adapt_event(&event, 123, 456);
            assert(event.type == (up ? SDL_MOUSEBUTTONUP : SDL_MOUSEBUTTONDOWN));
            assert(event.button.button == (i ? SDL_BUTTON_RIGHT : SDL_BUTTON_LEFT));
            assert(event.button.state == (up ? SDL_RELEASED : SDL_PRESSED));
            assert(event.button.x == 123 && event.button.y == 456);
        }
    }
    event.type = SDL_QUIT;
    rich4_sdl1_miyoo_adapt_event(&event, 0, 0);
    assert(event.type == SDL_QUIT);
    rich4_sdl1_miyoo_adapt_event(NULL, 0, 0);
    puts("miyoo input: ok");
    return 0;
}
