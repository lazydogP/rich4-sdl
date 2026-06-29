#include "platform/sdl1/text/text_backend_selector.h"
#include <stdlib.h>
#include <string.h>
#include "platform/platform_profile.h"
#include "platform/sdl1/text/text_backend_freetype.h"

int rich4_text_backend_uses_atlas(void)
{
#if defined(RICH4_PLATFORM_MIYOO) && defined(HAVE_SDL_TTF)
    const char *choice = getenv("RICH4_TEXT_BACKEND");
    return choice != NULL && strcmp(choice, "atlas") == 0;
#else
    return RICH4_TEXT_BACKEND == RICH4_TEXT_BACKEND_ATLAS;
#endif
}

void rich4_text_backend_set_font_path(const char *font_path)
{
#if RICH4_TEXT_BACKEND == RICH4_TEXT_BACKEND_FREETYPE || (defined(RICH4_PLATFORM_MIYOO) && defined(HAVE_SDL_TTF))
    rich4_text_backend_freetype_set_font_path(font_path);
#else
    (void)font_path;
#endif
}

int rich4_text_backend_init_for_profile(Rich4TextBackend *backend,
                                        const Rich4FontAtlas *atlas)
{
#if RICH4_TEXT_BACKEND == RICH4_TEXT_BACKEND_ATLAS
    if (rich4_text_backend_uses_atlas())
        return rich4_text_backend_atlas_init(backend, atlas);
#else
    (void)atlas;
#endif
#if RICH4_TEXT_BACKEND == RICH4_TEXT_BACKEND_FREETYPE || (defined(RICH4_PLATFORM_MIYOO) && defined(HAVE_SDL_TTF))
    if (!rich4_text_backend_freetype_init(backend)) return 0;
#if defined(RICH4_PLATFORM_MIYOO)
    {
        /* Verify the packaged font now so the runtime log reports actual
         * availability, not just successful callback installation. */
        const Rich4TextStyle style = {16, 0, 1, 0, 0xffff, 0};
        return rich4_text_backend_measure(backend, "A", &style, NULL, NULL);
    }
#else
    return 1;
#endif
#else
    return 0;
#endif
}
