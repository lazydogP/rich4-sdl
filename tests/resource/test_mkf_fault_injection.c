#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "resource/mkf.h"
#include "resource/mkf_test.h"

static void put_u32(unsigned char *p, uint32_t value)
{
    p[0] = (unsigned char)value;
    p[1] = (unsigned char)(value >> 8);
    p[2] = (unsigned char)(value >> 16);
    p[3] = (unsigned char)(value >> 24);
}

static void make_archive(const char *path, uint32_t unpacked, uint32_t packed)
{
    unsigned char data[64];
    uint32_t index_offset = 4 + 16 + packed;
    FILE *fp;
    memset(data, 0, sizeof(data));
    put_u32(data, index_offset);
    put_u32(data + 4, unpacked);
    put_u32(data + 8, packed);
    put_u32(data + 12, 0);
    put_u32(data + 16, unpacked);
    memset(data + 20, 0x5a, packed);
    put_u32(data + index_offset, 4);
    fp = fopen(path, "wb");
    assert(fp != NULL);
    assert(fwrite(data, 1, index_offset + 4, fp) == index_offset + 4);
    assert(fclose(fp) == 0);
}

static void expect_open_fault(const char *path, Rich4MkfError expected)
{
    Rich4Mkf *mkf = NULL;
    assert(rich4_mkf_open(path, &mkf) == expected);
    rich4_mkf_close(mkf);
    rich4_mkf_test_clear_failures();
}

static void expect_read_fault(Rich4Mkf *mkf, Rich4MkfError expected)
{
    unsigned char *data = (unsigned char *)(uintptr_t)1;
    size_t size = 123;
    assert(rich4_mkf_read_resource(mkf, 0, &data, &size, NULL) == expected);
    assert(data == NULL && size == 0);
    rich4_mkf_test_clear_failures();
}

int main(void)
{
    const char *plain_path = "build/mkf_fault_plain.mkf";
    const char *packed_path = "build/mkf_fault_packed.mkf";
    Rich4Mkf *plain = NULL;
    Rich4Mkf *packed = NULL;

    make_archive(plain_path, 4, 4);
    make_archive(packed_path, 4, 2);

    rich4_mkf_test_fail_alloc_after(0);
    expect_open_fault(plain_path, RICH4_MKF_ERR_MEMORY);
    rich4_mkf_test_fail_alloc_after(1);
    expect_open_fault(plain_path, RICH4_MKF_ERR_MEMORY);

    rich4_mkf_test_fail_seek_after(0);
    expect_open_fault(plain_path, RICH4_MKF_ERR_IO);
    rich4_mkf_test_fail_seek_after(1);
    expect_open_fault(plain_path, RICH4_MKF_ERR_IO);
    rich4_mkf_test_fail_read_after(0);
    expect_open_fault(plain_path, RICH4_MKF_ERR_IO);
    rich4_mkf_test_fail_read_after(1);
    expect_open_fault(plain_path, RICH4_MKF_ERR_IO);

    assert(rich4_mkf_open(plain_path, &plain) == RICH4_MKF_OK);
    assert(rich4_mkf_open(packed_path, &packed) == RICH4_MKF_OK);

    rich4_mkf_test_fail_alloc_after(0);
    expect_read_fault(plain, RICH4_MKF_ERR_MEMORY);
    rich4_mkf_test_fail_alloc_after(1);
    expect_read_fault(packed, RICH4_MKF_ERR_MEMORY);

    rich4_mkf_test_fail_seek_after(0);
    expect_read_fault(plain, RICH4_MKF_ERR_IO);
    rich4_mkf_test_fail_seek_after(1);
    expect_read_fault(plain, RICH4_MKF_ERR_IO);
    rich4_mkf_test_fail_read_after(0);
    expect_read_fault(plain, RICH4_MKF_ERR_IO);
    rich4_mkf_test_fail_read_after(1);
    expect_read_fault(plain, RICH4_MKF_ERR_IO);
    rich4_mkf_test_fail_read_after(1);
    expect_read_fault(packed, RICH4_MKF_ERR_IO);

    rich4_mkf_close(packed);
    rich4_mkf_close(plain);
    assert(remove(packed_path) == 0);
    assert(remove(plain_path) == 0);
    puts("MKF fault injection: ok");
    return 0;
}
