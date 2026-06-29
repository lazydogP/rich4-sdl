#include "resource/mkf.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "resource/mkf_decompress.h"
#include "resource/mkf_test.h"
#include "resource/byte_reader.h"

#ifdef RICH4_MKF_TESTING
static int test_alloc_failure = -1;
static int test_read_failure = -1;
static int test_seek_failure = -1;

static int test_should_fail(int *counter)
{
    if (*counter < 0) return 0;
    if (*counter == 0) {
        *counter = -1;
        return 1;
    }
    (*counter)--;
    return 0;
}

void rich4_mkf_test_fail_alloc_after(int count) { test_alloc_failure = count; }
void rich4_mkf_test_fail_read_after(int count) { test_read_failure = count; }
void rich4_mkf_test_fail_seek_after(int count) { test_seek_failure = count; }
void rich4_mkf_test_clear_failures(void)
{
    test_alloc_failure = test_read_failure = test_seek_failure = -1;
}
#endif

static void *mkf_calloc(size_t count, size_t size)
{
#ifdef RICH4_MKF_TESTING
    if (test_should_fail(&test_alloc_failure)) return NULL;
#endif
    return calloc(count, size);
}

static void *mkf_malloc(size_t size)
{
#ifdef RICH4_MKF_TESTING
    if (test_should_fail(&test_alloc_failure)) return NULL;
#endif
    return malloc(size);
}

struct Rich4Mkf {
    FILE *fp;
    char path[512];
    char error[256];
    uint32_t file_size;
    uint32_t index_offset;
    size_t count;
    uint32_t *offsets;
};

static void set_error(Rich4Mkf *mkf, const char *message)
{
    if (mkf != NULL) {
        snprintf(mkf->error, sizeof(mkf->error), "%s", message);
    }
}

static void set_errorf(Rich4Mkf *mkf, const char *prefix, const char *detail)
{
    if (mkf != NULL) {
        snprintf(mkf->error, sizeof(mkf->error), "%s: %s", prefix, detail);
    }
}

static int read_exact(FILE *fp, void *buf, size_t size)
{
#ifdef RICH4_MKF_TESTING
    if (test_should_fail(&test_read_failure)) return 0;
#endif
    return fread(buf, 1, size, fp) == size;
}

static Rich4MkfError seek_to(FILE *fp, uint32_t offset)
{
#ifdef RICH4_MKF_TESTING
    if (test_should_fail(&test_seek_failure)) return RICH4_MKF_ERR_IO;
#endif
    return fseek(fp, (long)offset, SEEK_SET) == 0 ? RICH4_MKF_OK : RICH4_MKF_ERR_IO;
}

const char *rich4_mkf_error_name(Rich4MkfError error)
{
    switch (error) {
    case RICH4_MKF_OK: return "ok";
    case RICH4_MKF_ERR_IO: return "io";
    case RICH4_MKF_ERR_FORMAT: return "format";
    case RICH4_MKF_ERR_RANGE: return "range";
    case RICH4_MKF_ERR_MEMORY: return "memory";
    case RICH4_MKF_ERR_DECOMPRESS: return "decompress";
    default: return "unknown";
    }
}

Rich4MkfError rich4_mkf_open(const char *path, Rich4Mkf **out_mkf)
{
    unsigned char head[4];
    Rich4Mkf *mkf;
    long size_long;
    size_t i;

    if (out_mkf == NULL) {
        return RICH4_MKF_ERR_FORMAT;
    }
    *out_mkf = NULL;

    mkf = (Rich4Mkf *)mkf_calloc(1, sizeof(*mkf));
    if (mkf == NULL) {
        return RICH4_MKF_ERR_MEMORY;
    }
    *out_mkf = mkf;
    snprintf(mkf->path, sizeof(mkf->path), "%s", path);

    mkf->fp = fopen(path, "rb");
    if (mkf->fp == NULL) {
        set_errorf(mkf, "cannot open MKF", strerror(errno));
        return RICH4_MKF_ERR_IO;
    }

    if (fseek(mkf->fp, 0, SEEK_END) != 0) {
        set_errorf(mkf, "cannot seek MKF", strerror(errno));
        return RICH4_MKF_ERR_IO;
    }
    size_long = ftell(mkf->fp);
    if (size_long < 0 || size_long > 0x7fffffffL) {
        set_error(mkf, "file size is unsupported");
        return RICH4_MKF_ERR_RANGE;
    }
    mkf->file_size = (uint32_t)size_long;
    if (mkf->file_size < 4) {
        set_error(mkf, "file is too small to contain an MKF header");
        return RICH4_MKF_ERR_FORMAT;
    }

    if (seek_to(mkf->fp, 0) != RICH4_MKF_OK || !read_exact(mkf->fp, head, sizeof(head))) {
        set_error(mkf, "cannot read MKF header");
        return RICH4_MKF_ERR_IO;
    }
    if (!rich4_byte_reader_le32(head, sizeof(head), 0, &mkf->index_offset)) {
        set_error(mkf, "cannot decode MKF header");
        return RICH4_MKF_ERR_FORMAT;
    }
    if (mkf->index_offset < 4 || mkf->index_offset >= mkf->file_size) {
        set_error(mkf, "index table offset is outside the file");
        return RICH4_MKF_ERR_RANGE;
    }
    if (((mkf->file_size - mkf->index_offset) % 4) != 0) {
        set_error(mkf, "index table length is not divisible by 4");
        return RICH4_MKF_ERR_FORMAT;
    }

    mkf->count = (mkf->file_size - mkf->index_offset) / 4;
    mkf->offsets = (uint32_t *)mkf_calloc(mkf->count, sizeof(uint32_t));
    if (mkf->offsets == NULL && mkf->count != 0) {
        set_error(mkf, "out of memory while reading index table");
        return RICH4_MKF_ERR_MEMORY;
    }

    if (seek_to(mkf->fp, mkf->index_offset) != RICH4_MKF_OK) {
        set_error(mkf, "cannot seek to index table");
        return RICH4_MKF_ERR_IO;
    }
    for (i = 0; i < mkf->count; i++) {
        unsigned char entry[4];
        if (!read_exact(mkf->fp, entry, sizeof(entry))) {
            set_error(mkf, "cannot read index table");
            return RICH4_MKF_ERR_IO;
        }
        if (!rich4_byte_reader_le32(entry, sizeof(entry), 0,
                                    &mkf->offsets[i])) {
            set_error(mkf, "cannot decode MKF index table");
            return RICH4_MKF_ERR_FORMAT;
        }
        if (mkf->offsets[i] < 4 || mkf->offsets[i] >= mkf->index_offset) {
            set_error(mkf, "resource offset is outside the resource area");
            return RICH4_MKF_ERR_RANGE;
        }
    }

    set_error(mkf, "ok");
    *out_mkf = mkf;
    return RICH4_MKF_OK;
}

void rich4_mkf_close(Rich4Mkf *mkf)
{
    if (mkf == NULL) {
        return;
    }
    if (mkf->fp != NULL) {
        fclose(mkf->fp);
    }
    free(mkf->offsets);
    free(mkf);
}

const char *rich4_mkf_last_error(const Rich4Mkf *mkf)
{
    return mkf != NULL ? mkf->error : "no MKF handle";
}

size_t rich4_mkf_count(const Rich4Mkf *mkf)
{
    return mkf != NULL ? mkf->count : 0;
}

Rich4MkfError rich4_mkf_get_info(const Rich4Mkf *mkf_const, size_t index,
                                 Rich4MkfResourceInfo *out_info)
{
    unsigned char header[16];
    Rich4Mkf *mkf = (Rich4Mkf *)mkf_const;

    if (mkf == NULL || out_info == NULL) {
        return RICH4_MKF_ERR_FORMAT;
    }
    if (index >= mkf->count) {
        set_error(mkf, "resource index is out of range");
        return RICH4_MKF_ERR_RANGE;
    }
    if (mkf->offsets[index] > mkf->index_offset - 16) {
        set_error(mkf, "resource header crosses the index table");
        return RICH4_MKF_ERR_RANGE;
    }
    if (seek_to(mkf->fp, mkf->offsets[index]) != RICH4_MKF_OK ||
        !read_exact(mkf->fp, header, sizeof(header))) {
        set_error(mkf, "cannot read resource header");
        return RICH4_MKF_ERR_IO;
    }

    out_info->offset = mkf->offsets[index];
    if (!rich4_byte_reader_le32(header, sizeof(header), 0,
                                &out_info->uncompressed_size) ||
        !rich4_byte_reader_le32(header, sizeof(header), 4,
                                &out_info->compressed_size) ||
        !rich4_byte_reader_le32(header, sizeof(header), 8,
                                &out_info->image_data_offset) ||
        !rich4_byte_reader_le32(header, sizeof(header), 12,
                                &out_info->image_data_size)) {
        set_error(mkf, "cannot decode resource header");
        return RICH4_MKF_ERR_FORMAT;
    }

    if (out_info->compressed_size > mkf->index_offset - out_info->offset - 16) {
        set_error(mkf, "compressed resource data crosses the index table");
        return RICH4_MKF_ERR_RANGE;
    }
    if (out_info->image_data_size > out_info->uncompressed_size ||
        (out_info->image_data_size != 0 &&
         out_info->image_data_offset > out_info->uncompressed_size - out_info->image_data_size)) {
        set_error(mkf, "image data range is outside the uncompressed resource");
        return RICH4_MKF_ERR_RANGE;
    }

    return RICH4_MKF_OK;
}

Rich4MkfError rich4_mkf_read_resource(Rich4Mkf *mkf, size_t index,
                                      unsigned char **out_data,
                                      size_t *out_size,
                                      Rich4MkfResourceInfo *out_info)
{
    Rich4MkfResourceInfo info;
    unsigned char *data;
    unsigned char *compressed;
    Rich4MkfError err;

    if (out_data == NULL || out_size == NULL) {
        return RICH4_MKF_ERR_FORMAT;
    }
    *out_data = NULL;
    *out_size = 0;

    err = rich4_mkf_get_info(mkf, index, &info);
    if (err != RICH4_MKF_OK) {
        return err;
    }
    if (info.uncompressed_size == 0) {
        if (info.compressed_size != 0) {
            set_error(mkf, "zero-length resource has compressed data");
            return RICH4_MKF_ERR_FORMAT;
        }
        if (out_info != NULL) {
            *out_info = info;
        }
        *out_data = NULL;
        *out_size = 0;
        return RICH4_MKF_OK;
    }
    if (info.compressed_size == 0) {
        set_error(mkf, "resource has zero compressed size");
        return RICH4_MKF_ERR_FORMAT;
    }

    data = (unsigned char *)mkf_malloc(info.uncompressed_size);
    if (data == NULL) {
        set_error(mkf, "out of memory while allocating resource");
        return RICH4_MKF_ERR_MEMORY;
    }

    if (seek_to(mkf->fp, info.offset + 16) != RICH4_MKF_OK) {
        free(data);
        set_error(mkf, "cannot seek to resource data");
        return RICH4_MKF_ERR_IO;
    }

    if (info.compressed_size == info.uncompressed_size) {
        if (!read_exact(mkf->fp, data, info.uncompressed_size)) {
            free(data);
            set_error(mkf, "cannot read uncompressed resource data");
            return RICH4_MKF_ERR_IO;
        }
    } else {
        compressed = (unsigned char *)mkf_calloc((size_t)info.compressed_size + 4, 1);
        if (compressed == NULL) {
            free(data);
            set_error(mkf, "out of memory while allocating compressed resource");
            return RICH4_MKF_ERR_MEMORY;
        }
        if (!read_exact(mkf->fp, compressed, info.compressed_size)) {
            free(compressed);
            free(data);
            set_error(mkf, "cannot read compressed resource data");
            return RICH4_MKF_ERR_IO;
        }
        mkf_decompress(data, compressed, info.uncompressed_size);
        free(compressed);
    }

    if (out_info != NULL) {
        *out_info = info;
    }
    *out_data = data;
    *out_size = info.uncompressed_size;
    return RICH4_MKF_OK;
}
