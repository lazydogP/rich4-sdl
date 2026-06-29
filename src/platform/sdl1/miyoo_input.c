#include "platform/sdl1/miyoo_input.h"

/* Physical key codes verified in the borrowed port's
 * kit/platform/miyoo_panel.h. Game actions use existing mouse callbacks and
 * the defaults at 0x47EDC2 (src/app/options_flow.c), plus the SDL dice seam
 * in game_loop_dice_interaction.c. This is a platform mapping. */
void rich4_sdl1_miyoo_adapt_event(SDL_Event *event, int cursor_x, int cursor_y)
{
    SDLKey key;
    Uint8 down;
    Uint8 button;
    if (event == NULL ||
        (event->type != SDL_KEYDOWN && event->type != SDL_KEYUP)) return;
    key = event->key.keysym.sym;
    down = event->type == SDL_KEYDOWN;
    if (key == SDLK_SPACE || key == SDLK_LCTRL) {
        button = key == SDLK_SPACE ? SDL_BUTTON_LEFT : SDL_BUTTON_RIGHT;
        event->type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
        event->button.which = 0;
        event->button.button = button;
        event->button.state = down ? SDL_PRESSED : SDL_RELEASED;
        event->button.x = (Uint16)cursor_x;
        event->button.y = (Uint16)cursor_y;
        return;
    }
    switch (key) {
    case SDLK_LSHIFT: key = SDLK_d; break;       /* X: dice */
    case SDLK_LALT: key = SDLK_RETURN; break;    /* Y: Enter */
    case SDLK_e: key = SDLK_PAGEUP; break;       /* L1 */
    case SDLK_t: key = SDLK_PAGEDOWN; break;     /* R1 */
    case SDLK_TAB: key = SDLK_c; break;          /* L2: cards */
    case SDLK_BACKSPACE: key = SDLK_w; break;    /* R2: tools */
    case SDLK_RCTRL: key = SDLK_TAB; break;      /* Select: view */
    default: break;                            /* Start: Enter; Menu: Esc */
    }
    event->key.keysym.sym = key;
    /* A held physical face button must not turn another button into a
     * Ctrl/Alt/Shift hotkey in the original game's configuration. */
    event->key.keysym.mod = KMOD_NONE;
    event->key.keysym.unicode = 0;
}
