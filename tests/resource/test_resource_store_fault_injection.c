#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "resource/resource_store.h"
#include "resource/resource_store_test.h"

static void put_u32(unsigned char *p, uint32_t value)
{
    p[0] = (unsigned char)value;
    p[1] = (unsigned char)(value >> 8);
    p[2] = (unsigned char)(value >> 16);
    p[3] = (unsigned char)(value >> 24);
}

static void make_archive(const char *path)
{
    unsigned char data[109];
    FILE *fp;
    int i;
    memset(data, 0, sizeof(data));
    put_u32(data, 89);
    for (i = 0; i < 5; i++) {
        size_t offset = 4 + (size_t)i * 17;
        put_u32(data + offset, 1);
        put_u32(data + offset + 4, 1);
        put_u32(data + offset + 8, 0);
        put_u32(data + offset + 12, 1);
        data[offset + 16] = (unsigned char)(0x40 + i);
        put_u32(data + 89 + i * 4, (uint32_t)offset);
    }
    fp = fopen(path, "wb");
    assert(fp != NULL);
    assert(fwrite(data, 1, sizeof(data), fp) == sizeof(data));
    assert(fclose(fp) == 0);
}

int main(void)
{
    const char *paths[5] = {
        "build/store_fault_0.mkf", "build/store_fault_1.mkf",
        "build/store_fault_2.mkf", "build/store_fault_3.mkf",
        "build/store_fault_4.mkf"
    };
    ResourceStore *store;
    Rich4Mkf *archives[5] = {NULL, NULL, NULL, NULL, NULL};
    Rich4ResourceHandle handle;
    int i;
    for (i = 0; i < 5; i++) make_archive(paths[i]);

    rich4_resource_store_test_fail_alloc_after(0);
    assert(rich4_resource_store_create() == NULL);
    rich4_resource_store_test_clear_failures();
    store = rich4_resource_store_create();
    assert(store != NULL);

    rich4_resource_store_test_fail_alloc_after(0);
    assert(rich4_resource_store_open_archive(store, paths[0], &archives[0]) ==
           RICH4_MKF_ERR_MEMORY);
    assert(archives[0] == NULL && rich4_resource_store_archive_count(store) == 0);

    rich4_resource_store_test_fail_alloc_after(1);
    assert(rich4_resource_store_open_archive(store, paths[0], &archives[0]) ==
           RICH4_MKF_ERR_MEMORY);
    assert(archives[0] == NULL && rich4_resource_store_archive_count(store) == 0);
    rich4_resource_store_test_clear_failures();
    assert(rich4_resource_store_open_archive(store, paths[0], &archives[0]) ==
           RICH4_MKF_OK);

    rich4_resource_store_test_fail_alloc_after(0);
    memset(&handle, 0xa5, sizeof(handle));
    assert(rich4_resource_store_borrow(store, archives[0], 0, &handle, NULL) ==
           RICH4_MKF_ERR_MEMORY);
    assert(!rich4_resource_handle_is_valid(&handle));
    assert(rich4_resource_store_cached_count(store) == 0);
    rich4_resource_store_test_clear_failures();
    for (i = 0; i < 4; i++) {
        assert(rich4_resource_store_borrow(store, archives[0], (size_t)i,
                                           &handle, NULL) == RICH4_MKF_OK);
    }
    assert(rich4_resource_store_cached_count(store) == 4);
    rich4_resource_store_test_fail_alloc_after(0);
    memset(&handle, 0xa5, sizeof(handle));
    assert(rich4_resource_store_borrow(store, archives[0], 4, &handle, NULL) ==
           RICH4_MKF_ERR_MEMORY);
    assert(!rich4_resource_handle_is_valid(&handle));
    assert(rich4_resource_store_cached_count(store) == 4);
    rich4_resource_store_test_clear_failures();
    assert(rich4_resource_store_borrow(store, archives[0], 4, &handle, NULL) ==
           RICH4_MKF_OK);

    for (i = 1; i < 4; i++) {
        assert(rich4_resource_store_open_archive(store, paths[i], &archives[i]) ==
               RICH4_MKF_OK);
    }
    assert(rich4_resource_store_archive_count(store) == 4);
    rich4_resource_store_test_fail_alloc_after(1);
    assert(rich4_resource_store_open_archive(store, paths[4], &archives[4]) ==
           RICH4_MKF_ERR_MEMORY);
    assert(archives[4] == NULL && rich4_resource_store_archive_count(store) == 4);
    rich4_resource_store_test_clear_failures();
    assert(rich4_resource_store_open_archive(store, paths[4], &archives[4]) ==
           RICH4_MKF_OK);
    rich4_resource_store_destroy(store);
    for (i = 0; i < 5; i++) assert(remove(paths[i]) == 0);
    puts("ResourceStore fault injection: ok");
    return 0;
}
