#ifndef RICH4_RESOURCE_STORE_H
#define RICH4_RESOURCE_STORE_H

#include <stddef.h>

#include "resource/mkf.h"
#include "api_contract.h"

typedef struct ResourceStore ResourceStore;

/* Optional diagnostic observer, called synchronously on cache hit (0),
 * before read/decode (1), and after read/decode (2). Never changes ownership.
 * The observer must not re-enter this store. */
typedef void (*Rich4ResourceDebugObserver)(void *context, int phase,
    const char *archive, size_t index, size_t bytes, Rich4MkfError error);
void rich4_resource_store_set_debug_observer(ResourceStore *store,
    Rich4ResourceDebugObserver observer, void *context);

typedef struct Rich4ResourceHandle {
    /* Permanent borrows have store lifetime; acquired views have lease lifetime. */
    RICH4_BORROWED_LIFETIME const unsigned char *data;
    size_t size;
    size_t archive_id;
    size_t resource_index;
    unsigned int generation;
} Rich4ResourceHandle;

/* A resource view never owns its bytes implicitly.  Loaders must carry this
 * value alongside the pointer so destruction is explicit and auditable. */
typedef enum Rich4AssetOwnership {
    RICH4_ASSET_BORROWED = 0,
    RICH4_ASSET_OWNED = 1
} Rich4AssetOwnership;

typedef struct Rich4RawAsset {
    const unsigned char *data;
    size_t size;
    Rich4ResourceHandle handle;
    Rich4AssetOwnership ownership;
} Rich4RawAsset;

RICH4_OWNED ResourceStore *rich4_resource_store_create(void);
/* Releases all owned archives and remaining cached resource bytes. */
RICH4_NULL_SAFE void rich4_resource_store_destroy(ResourceStore *store);

Rich4MkfError rich4_resource_store_open_archive(ResourceStore *store,
                                                const char *path,
                                                Rich4Mkf **out_mkf);
Rich4MkfError rich4_resource_store_borrow(ResourceStore *store,
                                         Rich4Mkf *mkf,
                                         size_t resource_index,
                                         Rich4ResourceHandle *out_handle,
                                         Rich4MkfResourceInfo *out_info);
/* Explicit scene leases. Unlike permanent borrows, their decoded bytes are
 * released after the final owner releases the handle. Never evict live views. */
Rich4MkfError rich4_resource_store_acquire(ResourceStore *store,
    Rich4Mkf *mkf, size_t resource_index, Rich4ResourceHandle *out_handle,
    Rich4MkfResourceInfo *out_info);
int rich4_resource_store_release(ResourceStore *store, Rich4ResourceHandle *handle);

int rich4_resource_handle_is_valid(const Rich4ResourceHandle *handle);
size_t rich4_resource_store_archive_count(const ResourceStore *store);
size_t rich4_resource_store_cached_count(const ResourceStore *store);

/* R10.3b: bounded-memory support for constrained platforms (e.g. Miyoo Mini).
 *
 * The decoded-resource cache is unbounded by default, matching the historical
 * borrow-forever contract (a Rich4ResourceHandle's bytes stay valid for the
 * store's lifetime). Set a byte ceiling to cap peak resident cache size.
 *
 * With eviction disabled (default), a borrow that would exceed the ceiling
 * fails with RICH4_MKF_ERR_MEMORY and never dangles an outstanding handle.
 *
 * Permanent borrows are never automatically evicted. Acquired resources are
 * freed by explicit release after their last scene owner finishes using them;
 * a bounded store rejects a live working set that does not fit. */
void rich4_resource_store_set_cache_limit(ResourceStore *store, size_t max_bytes);
/* Production profile guard: raw handles have store lifetime, so a profile
 * cannot enable entry eviction until a lease/reborrow contract exists. */
int rich4_resource_store_configure_borrow_policy(
    ResourceStore *store, size_t max_bytes, int enable_eviction);
size_t rich4_resource_store_cached_bytes(const ResourceStore *store);

#endif
