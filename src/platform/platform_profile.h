#ifndef RICH4_PLATFORM_PROFILE_H
#define RICH4_PLATFORM_PROFILE_H

#include <stddef.h>

/* R10.3a: platform capability / memory-budget profiles.
 *
 * A profile bundles the decisions that differ between a resource-rich desktop
 * and a constrained handheld (e.g. Miyoo Mini, 128 MB RAM): the resource cache
 * ceiling, whether to preload all assets, and the default asset locations.
 * Select the handheld profile at build time with
 * -DRICH4_PLATFORM_HANDHELD; desktop is the default.
 *
 * This header only declares budgets/selectors; consumers (resource_store, the
 * loader reads these macros so platform assumptions live in exactly one
 * place. */

#if defined(RICH4_PLATFORM_HANDHELD)

#define RICH4_PROFILE_NAME "handheld"
/* A raw borrow has store lifetime, not entry lifetime.  Until a lease API
 * exists, production profiles must not evict bytes behind a live borrow. */
#define RICH4_RESOURCE_CACHE_MAX_BYTES ((size_t)0)
#define RICH4_RESOURCE_EVICTION_ENABLED 0
/* Do not eagerly preload every MKF resource on the handheld. */
#define RICH4_RESOURCE_PRELOAD_ALL 0

#else /* desktop (default) */

#define RICH4_PLATFORM_DESKTOP 1
#define RICH4_PROFILE_NAME "desktop"
/* 0 means "unbounded" – desktop keeps the historical behavior. */
#define RICH4_RESOURCE_CACHE_MAX_BYTES ((size_t)0)
#define RICH4_RESOURCE_EVICTION_ENABLED 0
#define RICH4_RESOURCE_PRELOAD_ALL 1

#endif

#endif
