#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "resource/resource_store.h"

typedef struct DebugReadCounts { int begin, end, hit, errors; } DebugReadCounts;
static void observe_read(void *context, int phase, const char *archive,
    size_t index, size_t bytes, Rich4MkfError error)
{
    DebugReadCounts *counts = context;
    assert(archive != NULL && strstr(archive, "resource_store_sample.mkf") != NULL);
    if (phase == 0) { assert(bytes == 4); counts->hit++; }
    if (phase == 1) counts->begin++;
    if (phase == 2) {
        assert(counts->begin == counts->end + 1);
        counts->end++;
        if (error != RICH4_MKF_OK) { assert(index == 1); counts->errors++; }
        else assert(bytes == 4);
    }
}

static void write_u32le(FILE *fp, uint32_t value)
{
    fputc((int)(value & 0xff), fp);
    fputc((int)((value >> 8) & 0xff), fp);
    fputc((int)((value >> 16) & 0xff), fp);
    fputc((int)((value >> 24) & 0xff), fp);
}

static void make_sample(const char *path)
{
    FILE *fp = fopen(path, "wb");
    assert(fp != NULL);
    write_u32le(fp, 24);
    write_u32le(fp, 4);
    write_u32le(fp, 4);
    write_u32le(fp, 0);
    write_u32le(fp, 4);
    fwrite("TEST", 1, 4, fp);
    write_u32le(fp, 4);
    assert(fclose(fp) == 0);
}

int main(void)
{
    const char *path = "build/resource_store_sample.mkf";
    ResourceStore *store;
    Rich4Mkf *first = NULL;
    Rich4Mkf *same = NULL;
    Rich4ResourceHandle a;
    Rich4ResourceHandle b;
    Rich4ResourceHandle invalid;
    int cycle;
    DebugReadCounts debug_counts = {0};

    make_sample(path);
    store = rich4_resource_store_create();
    assert(store != NULL);
    rich4_resource_store_set_debug_observer(store, observe_read, &debug_counts);
    assert(rich4_resource_store_open_archive(store, path, &first) == RICH4_MKF_OK);
    assert(rich4_resource_store_open_archive(store, path, &same) == RICH4_MKF_OK);
    assert(first == same);
    assert(rich4_resource_store_archive_count(store) == 1);
    assert(rich4_resource_store_borrow(store, first, 0, &a, NULL) == RICH4_MKF_OK);
    assert(rich4_resource_handle_is_valid(&a));
    assert(a.size == 4 && a.data[0] == 'T');
    assert(rich4_resource_store_borrow(store, first, 0, &b, NULL) == RICH4_MKF_OK);
    assert(a.data == b.data);
    assert(rich4_resource_store_cached_count(store) == 1);
    assert(rich4_resource_store_borrow(store, first, 1, &invalid, NULL) == RICH4_MKF_ERR_RANGE);
    assert(!rich4_resource_handle_is_valid(&invalid));
    assert(rich4_resource_store_cached_count(store) == 1);
    rich4_resource_store_destroy(store);
    assert(debug_counts.begin == 2 && debug_counts.end == 2);
    assert(debug_counts.hit == 1 && debug_counts.errors == 1);

    /* Two live leases share bytes; the final release recovers the cache.
     * Reacquisition has a new generation so an old copied handle cannot
     * release the replacement allocation. Permanent borrows stay pinned. */
    store = rich4_resource_store_create();
    assert(rich4_resource_store_open_archive(store, path, &first) == RICH4_MKF_OK);
    assert(rich4_resource_store_acquire(store, first, 0, &a, NULL) == RICH4_MKF_OK);
    assert(rich4_resource_store_acquire(store, first, 0, &b, NULL) == RICH4_MKF_OK);
    invalid = a;
    assert(rich4_resource_store_release(store, &a));
    assert(!rich4_resource_store_release(store, &a));
    assert(b.data[3] == 'T' && rich4_resource_store_cached_bytes(store) == 4);
    assert(rich4_resource_store_release(store, &b));
    assert(rich4_resource_store_cached_bytes(store) == 0);
    assert(rich4_resource_store_acquire(store, first, 0, &a, NULL) == RICH4_MKF_OK);
    assert(a.generation != invalid.generation);
    assert(!rich4_resource_store_release(store, &invalid));
    assert(rich4_resource_store_borrow(store, first, 0, &b, NULL) == RICH4_MKF_OK);
    assert(rich4_resource_store_release(store, &a));
    assert(b.data[0] == 'T' && rich4_resource_store_cached_bytes(store) == 4);
    rich4_resource_store_destroy(store);

    /* Exercise the complete archive/cache lifetime repeatedly.  Sanitizer
     * builds turn this loop into a leak/use-after-free gate. */
    for (cycle = 0; cycle < 32; cycle++) {
        store = rich4_resource_store_create();
        assert(store != NULL);
        assert(rich4_resource_store_open_archive(store, path, &first) == RICH4_MKF_OK);
        assert(rich4_resource_store_borrow(store, first, 0, &a, NULL) == RICH4_MKF_OK);
        assert(a.size == 4 && a.data[3] == 'T');
        rich4_resource_store_destroy(store);
    }

    store = rich4_resource_store_create();
    assert(store != NULL);
    assert(rich4_resource_store_open_archive(store,
                                             "build/does-not-exist.mkf",
                                             &first) == RICH4_MKF_ERR_IO);
    assert(first == NULL);
    assert(rich4_resource_store_archive_count(store) == 0);
    rich4_resource_store_destroy(store);
    rich4_resource_store_destroy(NULL);
    assert(remove(path) == 0);
    puts("resource store tests passed");
    return 0;
}
