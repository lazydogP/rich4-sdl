#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "resource/resource_store.h"

/* R10.3b: exercise the configurable cache ceiling, byte accounting, hard-cap
 * failure path, and the explicit no-eviction policy. */

static void write_u32le(FILE *fp, uint32_t value)
{
    fputc((int)(value & 0xff), fp);
    fputc((int)((value >> 8) & 0xff), fp);
    fputc((int)((value >> 16) & 0xff), fp);
    fputc((int)((value >> 24) & 0xff), fp);
}

/* Two 4-byte resources ("AAAA" at index 0, "BBBB" at index 1). Layout:
 *   [0..4)   index_offset = 44
 *   [4..24)  resource 0: 16-byte header + "AAAA"
 *   [24..44) resource 1: 16-byte header + "BBBB"
 *   [44..52) index table: offsets {4, 24}
 */
static void make_sample2(const char *path)
{
    FILE *fp = fopen(path, "wb");
    assert(fp != NULL);
    write_u32le(fp, 44); /* index_offset */
    /* resource 0 header + data */
    write_u32le(fp, 4);  /* uncompressed_size */
    write_u32le(fp, 4);  /* compressed_size (stored) */
    write_u32le(fp, 0);  /* image_data_offset */
    write_u32le(fp, 4);  /* image_data_size */
    fwrite("AAAA", 1, 4, fp);
    /* resource 1 header + data */
    write_u32le(fp, 4);
    write_u32le(fp, 4);
    write_u32le(fp, 0);
    write_u32le(fp, 4);
    fwrite("BBBB", 1, 4, fp);
    /* index table */
    write_u32le(fp, 4);
    write_u32le(fp, 24);
    assert(fclose(fp) == 0);
}

int main(void)
{
    const char *path = "build/resource_store_cache_sample.mkf";
    ResourceStore *store;
    Rich4Mkf *mkf = NULL;
    Rich4ResourceHandle a;
    Rich4ResourceHandle b;

    make_sample2(path);

    /* Unbounded (default): both resources cached, bytes accounted. */
    store = rich4_resource_store_create();
    assert(store != NULL);
    assert(rich4_resource_store_open_archive(store, path, &mkf) == RICH4_MKF_OK);
    assert(rich4_resource_store_borrow(store, mkf, 0, &a, NULL) == RICH4_MKF_OK);
    assert(a.data[0] == 'A');
    assert(rich4_resource_store_cached_bytes(store) == 4);
    assert(rich4_resource_store_borrow(store, mkf, 1, &b, NULL) == RICH4_MKF_OK);
    assert(b.data[0] == 'B');
    assert(rich4_resource_store_cached_count(store) == 2);
    assert(rich4_resource_store_cached_bytes(store) == 8);
    rich4_resource_store_destroy(store);

    /* Hard cap, eviction disabled: second distinct borrow fails cleanly. */
    store = rich4_resource_store_create();
    assert(!rich4_resource_store_configure_borrow_policy(store, 4, 1));
    assert(rich4_resource_store_configure_borrow_policy(store, 4, 0));
    assert(rich4_resource_store_open_archive(store, path, &mkf) == RICH4_MKF_OK);
    rich4_resource_store_set_cache_limit(store, 4);
    assert(rich4_resource_store_borrow(store, mkf, 0, &a, NULL) == RICH4_MKF_OK);
    assert(rich4_resource_store_cached_bytes(store) == 4);
    assert(rich4_resource_store_borrow(store, mkf, 1, &b, NULL) == RICH4_MKF_ERR_MEMORY);
    /* cache unchanged and no dangling entry added */
    assert(rich4_resource_store_cached_count(store) == 1);
    assert(rich4_resource_store_cached_bytes(store) == 4);
    /* re-borrowing the cached entry still succeeds */
    assert(rich4_resource_store_borrow(store, mkf, 0, &a, NULL) == RICH4_MKF_OK);
    rich4_resource_store_destroy(store);

    assert(remove(path) == 0);
    puts("resource store cache tests passed");
    return 0;
}
