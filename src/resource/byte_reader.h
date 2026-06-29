#ifndef RICH4_RESOURCE_BYTE_READER_H
#define RICH4_RESOURCE_BYTE_READER_H

#include <stddef.h>
#include <stdint.h>

/* Bounds-checked little-endian reads for decoded resource bytes.  The caller
 * supplies the complete buffer length and an offset; no pointer arithmetic is
 * performed until the requested width has been proven to fit. */
int rich4_byte_reader_le16(const unsigned char *data, size_t size,
                           size_t offset, uint16_t *out_value);
int rich4_byte_reader_le32(const unsigned char *data, size_t size,
                           size_t offset, uint32_t *out_value);

#endif
