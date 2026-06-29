#include "resource/resource_store.h"
#include "resource/resource_store_test.h"

#include <stdlib.h>
#include <string.h>

#ifdef RICH4_RESOURCE_STORE_TESTING
static int test_alloc_failure = -1;
void rich4_resource_store_test_fail_alloc_after(int count)
{
    test_alloc_failure = count;
}
void rich4_resource_store_test_clear_failures(void)
{
    test_alloc_failure = -1;
}
#endif

static int store_alloc_should_fail(void)
{
#ifdef RICH4_RESOURCE_STORE_TESTING
    if (test_alloc_failure == 0) {
        test_alloc_failure = -1;
        return 1;
    }
    if (test_alloc_failure > 0) test_alloc_failure--;
#endif
    return 0;
}

static void *store_calloc(size_t count, size_t size)
{
    return store_alloc_should_fail() ? NULL : calloc(count, size);
}

static void *store_malloc(size_t size)
{
    return store_alloc_should_fail() ? NULL : malloc(size);
}

static void *store_realloc(void *memory, size_t size)
{
    return store_alloc_should_fail() ? NULL : realloc(memory, size);
}

typedef struct ResourceArchiveEntry {
    char *path;
    Rich4Mkf *mkf;
} ResourceArchiveEntry;

typedef struct ResourceCacheEntry {
    size_t archive_id;
    size_t resource_index;
    unsigned char *data;
    size_t size;
    Rich4MkfResourceInfo info;
    size_t leases;
    int permanent;
    unsigned int generation;
} ResourceCacheEntry;

struct ResourceStore {
    ResourceArchiveEntry *archives;
    size_t archive_count;
    size_t archive_capacity;
    ResourceCacheEntry *cache;
    size_t cache_count;
    size_t cache_capacity;
    size_t cache_bytes;       /* R10.3b: total bytes of cached resource data */
    size_t cache_limit_bytes; /* R10.3b: 0 = unbounded */
    unsigned int generation;
    Rich4ResourceDebugObserver debug_observer;
    void *debug_context;
};

void rich4_resource_store_set_debug_observer(ResourceStore *store,
    Rich4ResourceDebugObserver observer, void *context)
{
    if (store != NULL) {
        store->debug_observer = observer;
        store->debug_context = context;
    }
}

static char *copy_string(const char *text)
{
    size_t size;
    char *copy;
    if (text == NULL) {
        return NULL;
    }
    size = strlen(text) + 1;
    copy = (char *)store_malloc(size);
    if (copy != NULL) {
        memcpy(copy, text, size);
    }
    return copy;
}

static int grow_array(void **items, size_t *capacity, size_t item_size)
{
    size_t next = *capacity == 0 ? 4 : *capacity * 2;
    void *grown;
    if (next < *capacity || item_size > (size_t)-1 / next) {
        return 0;
    }
    grown = store_realloc(*items, next * item_size);
    if (grown == NULL) {
        return 0;
    }
    *items = grown;
    *capacity = next;
    return 1;
}

ResourceStore *rich4_resource_store_create(void)
{
    ResourceStore *store = (ResourceStore *)store_calloc(1, sizeof(*store));
    if (store != NULL) {
        store->generation = 1;
    }
    return store;
}

void rich4_resource_store_destroy(ResourceStore *store)
{
    size_t i;
    if (store == NULL) {
        return;
    }
    for (i = 0; i < store->cache_count; i++) {
        free(store->cache[i].data);
    }
    for (i = 0; i < store->archive_count; i++) {
        rich4_mkf_close(store->archives[i].mkf);
        free(store->archives[i].path);
    }
    free(store->cache);
    free(store->archives);
    free(store);
}

Rich4MkfError rich4_resource_store_open_archive(ResourceStore *store,
                                                const char *path,
                                                Rich4Mkf **out_mkf)
{
    Rich4Mkf *mkf = NULL;
    Rich4MkfError err;
    char *path_copy;
    size_t i;
    if (out_mkf != NULL) {
        *out_mkf = NULL;
    }
    if (store == NULL || path == NULL || out_mkf == NULL) {
        return RICH4_MKF_ERR_FORMAT;
    }
    for (i = 0; i < store->archive_count; i++) {
        if (strcmp(store->archives[i].path, path) == 0) {
            *out_mkf = store->archives[i].mkf;
            return RICH4_MKF_OK;
        }
    }
    err = rich4_mkf_open(path, &mkf);
    if (err != RICH4_MKF_OK) {
        rich4_mkf_close(mkf);
        return err;
    }
    path_copy = copy_string(path);
    if (path_copy == NULL ||
        (store->archive_count == store->archive_capacity &&
         !grow_array((void **)&store->archives,
                     &store->archive_capacity,
                     sizeof(*store->archives)))) {
        free(path_copy);
        rich4_mkf_close(mkf);
        return RICH4_MKF_ERR_MEMORY;
    }
    store->archives[store->archive_count].path = path_copy;
    store->archives[store->archive_count].mkf = mkf;
    store->archive_count++;
    *out_mkf = mkf;
    return RICH4_MKF_OK;
}

static Rich4MkfError store_get(ResourceStore *store,
                                         Rich4Mkf *mkf,
                                         size_t resource_index,
                                         Rich4ResourceHandle *out_handle,
                                         Rich4MkfResourceInfo *out_info,
                                         int lease)
{
    size_t archive_id;
    size_t i;
    ResourceCacheEntry entry;
    Rich4MkfError err;
    if (out_handle != NULL) {
        memset(out_handle, 0, sizeof(*out_handle));
    }
    if (store == NULL || mkf == NULL || out_handle == NULL) {
        return RICH4_MKF_ERR_FORMAT;
    }
    for (archive_id = 0; archive_id < store->archive_count; archive_id++) {
        if (store->archives[archive_id].mkf == mkf) {
            break;
        }
    }
    if (archive_id == store->archive_count) {
        return RICH4_MKF_ERR_FORMAT;
    }
    for (i = 0; i < store->cache_count; i++) {
        if (store->cache[i].archive_id == archive_id &&
            store->cache[i].resource_index == resource_index) {
            entry = store->cache[i];
            if (store->debug_observer != NULL)
                store->debug_observer(store->debug_context, 0,
                    store->archives[archive_id].path, resource_index,
                    entry.size, RICH4_MKF_OK);
            goto borrowed;
        }
    }
    memset(&entry, 0, sizeof(entry));
    entry.archive_id = archive_id;
    entry.resource_index = resource_index;
    if (store->debug_observer != NULL)
        store->debug_observer(store->debug_context, 1,
            store->archives[archive_id].path, resource_index, 0, RICH4_MKF_OK);
    err = rich4_mkf_read_resource(mkf,
                                  resource_index,
                                  &entry.data,
                                  &entry.size,
                                  &entry.info);
    if (store->debug_observer != NULL)
        store->debug_observer(store->debug_context, 2,
            store->archives[archive_id].path, resource_index, entry.size, err);
    if (err != RICH4_MKF_OK) {
        free(entry.data);
        return err;
    }
    /* R10.3b: honor the configurable cache ceiling. */
    if (store->cache_limit_bytes != 0 &&
        store->cache_bytes + entry.size > store->cache_limit_bytes) {
        free(entry.data);
        return RICH4_MKF_ERR_MEMORY;
    }
    if (store->cache_count == store->cache_capacity &&
        !grow_array((void **)&store->cache,
                    &store->cache_capacity,
                    sizeof(*store->cache))) {
        free(entry.data);
        return RICH4_MKF_ERR_MEMORY;
    }
    entry.generation = store->generation++;
    if (store->generation == 0) store->generation = 1;
    i = store->cache_count;
    store->cache[store->cache_count++] = entry;
    store->cache_bytes += entry.size;
borrowed:
    if (lease) store->cache[i].leases++;
    else store->cache[i].permanent = 1;
    out_handle->data = entry.data;
    out_handle->size = entry.size;
    out_handle->archive_id = entry.archive_id;
    out_handle->resource_index = entry.resource_index;
    out_handle->generation = entry.generation;
    if (out_info != NULL) {
        *out_info = entry.info;
    }
    return RICH4_MKF_OK;
}

Rich4MkfError rich4_resource_store_borrow(ResourceStore *store, Rich4Mkf *mkf,
    size_t index, Rich4ResourceHandle *handle, Rich4MkfResourceInfo *info)
{
    return store_get(store, mkf, index, handle, info, 0);
}

Rich4MkfError rich4_resource_store_acquire(ResourceStore *store, Rich4Mkf *mkf,
    size_t index, Rich4ResourceHandle *handle, Rich4MkfResourceInfo *info)
{
    return store_get(store, mkf, index, handle, info, 1);
}

int rich4_resource_store_release(ResourceStore *store, Rich4ResourceHandle *handle)
{
    size_t i;
    if (store == NULL || handle == NULL || !handle->generation) return 0;
    for (i = 0; i < store->cache_count; i++) {
        ResourceCacheEntry *entry = &store->cache[i];
        if (entry->archive_id != handle->archive_id ||
            entry->resource_index != handle->resource_index ||
            entry->generation != handle->generation ||
            entry->data != handle->data || entry->leases == 0) continue;
        entry->leases--;
        if (!entry->leases && !entry->permanent) {
            store->cache_bytes -= entry->size;
            free(entry->data);
            memmove(entry, entry + 1, (store->cache_count - i - 1) * sizeof(*entry));
            store->cache_count--;
        }
        memset(handle, 0, sizeof(*handle));
        return 1;
    }
    return 0;
}

int rich4_resource_handle_is_valid(const Rich4ResourceHandle *handle)
{
    return handle != NULL && handle->generation != 0;
}

size_t rich4_resource_store_archive_count(const ResourceStore *store)
{
    return store == NULL ? 0 : store->archive_count;
}

size_t rich4_resource_store_cached_count(const ResourceStore *store)
{
    return store == NULL ? 0 : store->cache_count;
}

void rich4_resource_store_set_cache_limit(ResourceStore *store, size_t max_bytes)
{
    if (store != NULL) {
        store->cache_limit_bytes = max_bytes;
    }
}

int rich4_resource_store_configure_borrow_policy(
    ResourceStore *store, size_t max_bytes, int enable_eviction)
{
    if (store == NULL || enable_eviction) return 0;
    store->cache_limit_bytes = max_bytes;
    return 1;
}

size_t rich4_resource_store_cached_bytes(const ResourceStore *store)
{
    return store == NULL ? 0 : store->cache_bytes;
}
