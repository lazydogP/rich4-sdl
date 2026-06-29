#include "resource/byte_reader.h"

static int byte_reader_range_ok(const unsigned char *data, size_t size,
                                size_t offset, size_t width)
{
    return data != NULL && offset <= size && width <= size - offset;
}

int rich4_byte_reader_le16(const unsigned char *data, size_t size,
                           size_t offset, uint16_t *out_value)
{
    const unsigned char *p;
    if (out_value == NULL || !byte_reader_range_ok(data, size, offset, 2))
        return 0;
    p = data + offset;
    *out_value = (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
    return 1;
}

int rich4_byte_reader_le32(const unsigned char *data, size_t size,
                           size_t offset, uint32_t *out_value)
{
    const unsigned char *p;
    if (out_value == NULL || !byte_reader_range_ok(data, size, offset, 4))
        return 0;
    p = data + offset;
    *out_value = (uint32_t)p[0] |
                 ((uint32_t)p[1] << 8) |
                 ((uint32_t)p[2] << 16) |
                 ((uint32_t)p[3] << 24);
    return 1;
}
