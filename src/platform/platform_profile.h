#ifndef RICH4_PLATFORM_PROFILE_H
#define RICH4_PLATFORM_PROFILE_H

#include <stddef.h>

/* R10.3a: platform capability / memory-budget profiles.
 *
 * A profile bundles the decisions that differ between a resource-rich desktop
 * and a constrained handheld (e.g. Miyoo Mini, 128 MB RAM): which text backend
 * to use, the resource cache ceiling, whether to preload all assets, and the
 * default asset locations. Select the handheld profile at build time with
 * -DRICH4_PLATFORM_HANDHELD; desktop is the default.
 *
 * This header only declares budgets/selectors; consumers (resource_store, the
 * text backend selector, the loader) read these macros so platform assumptions
 * live in exactly one place. */

#define RICH4_TEXT_BACKEND_FREETYPE 0 /* desktop: SDL_ttf / FreeType */
#define RICH4_TEXT_BACKEND_ATLAS    1 /* handheld: prebaked bitmap atlas */

#if defined(RICH4_PLATFORM_HANDHELD)

#define RICH4_PROFILE_NAME "handheld"
#define RICH4_TEXT_BACKEND RICH4_TEXT_BACKEND_ATLAS
/* A raw borrow has store lifetime, not entry lifetime.  Until a lease API
 * exists, production profiles must not evict bytes behind a live borrow. */
#define RICH4_RESOURCE_CACHE_MAX_BYTES ((size_t)0)
#define RICH4_RESOURCE_EVICTION_ENABLED 0
/* Do not eagerly preload every MKF resource on the handheld. */
#define RICH4_RESOURCE_PRELOAD_ALL 0
#define RICH4_FONT_ATLAS_PATH "assets/fonts/rich4_font.r4atlas"

#else /* desktop (default) */

#define RICH4_PLATFORM_DESKTOP 1
#define RICH4_PROFILE_NAME "desktop"
#define RICH4_TEXT_BACKEND RICH4_TEXT_BACKEND_FREETYPE
/* 0 means "unbounded" – desktop keeps the historical behavior. */
#define RICH4_RESOURCE_CACHE_MAX_BYTES ((size_t)0)
#define RICH4_RESOURCE_EVICTION_ENABLED 0
#define RICH4_RESOURCE_PRELOAD_ALL 1
#define RICH4_FONT_ATLAS_PATH "assets/fonts/rich4_font.r4atlas"

#endif

#endif
