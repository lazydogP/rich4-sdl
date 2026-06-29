#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "resource/mkf.h"

static void put_u32(unsigned char *p, uint32_t value)
{
    p[0] = (unsigned char)value;
    p[1] = (unsigned char)(value >> 8);
    p[2] = (unsigned char)(value >> 16);
    p[3] = (unsigned char)(value >> 24);
}

static void write_file(const char *path, const unsigned char *data, size_t size)
{
    FILE *fp = fopen(path, "wb");
    assert(fp != NULL);
    assert(fwrite(data, 1, size, fp) == size);
    assert(fclose(fp) == 0);
}

static void expect_open_error(const char *path,
                              const unsigned char *data,
                              size_t size,
                              Rich4MkfError expected)
{
    Rich4Mkf *mkf = (Rich4Mkf *)(uintptr_t)1;
    write_file(path, data, size);
    assert(rich4_mkf_open(path, &mkf) == expected);
    assert(mkf != NULL);
    assert(strcmp(rich4_mkf_last_error(mkf), "ok") != 0);
    rich4_mkf_close(mkf);
    assert(remove(path) == 0);
}

int main(void)
{
    const char *path = "build/mkf_failure_sample.mkf";
    unsigned char data[32];
    Rich4Mkf *mkf = NULL;
    unsigned char *out_data = (unsigned char *)(uintptr_t)1;
    size_t out_size = 99;
    Rich4MkfResourceInfo info;

    assert(rich4_mkf_open(path, NULL) == RICH4_MKF_ERR_FORMAT);
    assert(rich4_mkf_get_info(NULL, 0, &info) == RICH4_MKF_ERR_FORMAT);
    assert(rich4_mkf_read_resource(NULL, 0, NULL, &out_size, NULL) ==
           RICH4_MKF_ERR_FORMAT);
    assert(rich4_mkf_count(NULL) == 0);
    rich4_mkf_close(NULL);

    memset(data, 0, sizeof(data));
    expect_open_error(path, data, 3, RICH4_MKF_ERR_FORMAT);

    put_u32(data, 3);
    expect_open_error(path, data, 8, RICH4_MKF_ERR_RANGE);

    memset(data, 0, sizeof(data));
    put_u32(data, 5);
    expect_open_error(path, data, 10, RICH4_MKF_ERR_FORMAT);

    memset(data, 0, sizeof(data));
    put_u32(data, 20);
    put_u32(data + 20, 3);
    expect_open_error(path, data, 24, RICH4_MKF_ERR_RANGE);

    /* Valid table, but the resource's 16-byte header crosses the table. */
    memset(data, 0, sizeof(data));
    put_u32(data, 20);
    put_u32(data + 20, 8);
    write_file(path, data, 24);
    assert(rich4_mkf_open(path, &mkf) == RICH4_MKF_OK);
    memset(&info, 0xa5, sizeof(info));
    assert(rich4_mkf_get_info(mkf, 0, &info) == RICH4_MKF_ERR_RANGE);
    assert(rich4_mkf_read_resource(mkf, 0, &out_data, &out_size, NULL) ==
           RICH4_MKF_ERR_RANGE);
    assert(out_data == NULL && out_size == 0);
    rich4_mkf_close(mkf);
    assert(remove(path) == 0);

    /* Valid resource header whose image subrange exceeds uncompressed data. */
    memset(data, 0, sizeof(data));
    put_u32(data, 24);
    put_u32(data + 4, 4);
    put_u32(data + 8, 4);
    put_u32(data + 12, 3);
    put_u32(data + 16, 2);
    put_u32(data + 24, 4);
    write_file(path, data, 28);
    assert(rich4_mkf_open(path, &mkf) == RICH4_MKF_OK);
    assert(rich4_mkf_get_info(mkf, 0, &info) == RICH4_MKF_ERR_RANGE);
    rich4_mkf_close(mkf);
    assert(remove(path) == 0);

    /* Zero uncompressed size cannot own compressed bytes. */
    memset(data, 0, sizeof(data));
    put_u32(data, 25);
    put_u32(data + 4, 0);
    put_u32(data + 8, 1);
    data[20] = 0xaa;
    put_u32(data + 25, 4);
    write_file(path, data, 29);
    assert(rich4_mkf_open(path, &mkf) == RICH4_MKF_OK);
    out_data = (unsigned char *)(uintptr_t)1;
    out_size = 99;
    assert(rich4_mkf_read_resource(mkf, 0, &out_data, &out_size, NULL) ==
           RICH4_MKF_ERR_FORMAT);
    assert(out_data == NULL && out_size == 0);
    rich4_mkf_close(mkf);
    assert(remove(path) == 0);

    puts("MKF failure paths: ok");
    return 0;
}
