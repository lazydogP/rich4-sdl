#ifndef RICH4_PLATFORM_SDL1_TEXT_BACKEND_ATLAS_H
#define RICH4_PLATFORM_SDL1_TEXT_BACKEND_ATLAS_H

#include "platform/sdl1/text/font_atlas.h"
#include "platform/sdl1/text/text_backend.h"

/* R10.2d: handheld text backend. Consumes a prebaked glyph atlas and needs no
 * font engine (no FreeType/GDI). The caller owns the atlas lifetime; the
 * backend borrows it. Returns non-zero on success. */
int rich4_text_backend_atlas_init(Rich4TextBackend *backend,
                                  const Rich4FontAtlas *atlas);

#endif
