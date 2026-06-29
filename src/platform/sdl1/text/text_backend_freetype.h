#ifndef RICH4_PLATFORM_SDL1_TEXT_BACKEND_FREETYPE_H
#define RICH4_PLATFORM_SDL1_TEXT_BACKEND_FREETYPE_H

#include "platform/sdl1/text/text_backend.h"

/* R10.2b: desktop text backend (SDL_ttf / FreeType). Initializes the vtable in
 * `backend`. Returns non-zero when the backend is functional (SDL_ttf present),
 * zero when it is only a non-functional stub so the caller can fall back. */
int rich4_text_backend_freetype_init(Rich4TextBackend *backend);

/* Select the application-provided font before the first measure/render call.
 * The backend still retains its platform fallbacks for a missing/bad file. */
void rich4_text_backend_freetype_set_font_path(const char *font_path);

#endif
