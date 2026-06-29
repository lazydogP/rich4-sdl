#ifndef RICH4_PLATFORM_SDL1_TEXT_BACKEND_SELECTOR_H
#define RICH4_PLATFORM_SDL1_TEXT_BACKEND_SELECTOR_H

#include "platform/sdl1/text/text_backend.h"
#include "platform/sdl1/text/text_backend_atlas.h"

int rich4_text_backend_uses_atlas(void);

/* Select a production text backend. Mini defaults to TTF when available. The
 * atlas is borrowed only by the handheld backend and may be NULL on desktop. */
int rich4_text_backend_init_for_profile(Rich4TextBackend *backend,
                                        const Rich4FontAtlas *atlas);
void rich4_text_backend_set_font_path(const char *font_path);

#endif
