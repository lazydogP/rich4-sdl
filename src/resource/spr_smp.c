#include "resource/spr_smp.h"

#include <limits.h>
#include <string.h>

#include "resource/byte_reader.h"

#define RICH4_SPR_SMP_HEADER_SIZE 12u
#define RICH4_SPR_SMP_CHUNK_ENTRY_SIZE 12u
#define RICH4_SPR_PALETTE_SIZE 512u

int rich4_spr_smp_probe(const unsigned char *data, size_t size,
                        Rich4SprSmpInfo *out_info)
{
    uint32_t num_chunks;
    uint32_t start_offset;

    if (data == NULL || size < RICH4_SPR_SMP_HEADER_SIZE) {
        return 0;
    }
    if (!(memcmp(data, "SPR", 3) == 0 || memcmp(data, "SMP", 3) == 0) ||
        data[3] != '\0') {
        return 0;
    }
    if (!rich4_byte_reader_le32(data, size, 4, &num_chunks) ||
        !rich4_byte_reader_le32(data, size, 8, &start_offset)) {
        return 0;
    }
    if ((size_t)num_chunks >
        (size - RICH4_SPR_SMP_HEADER_SIZE) / RICH4_SPR_SMP_CHUNK_ENTRY_SIZE) {
        return 0;
    }
    if (out_info != NULL) {
        memcpy(out_info->signature, data, 4);
        out_info->num_chunks = num_chunks;
        out_info->start_offset = start_offset;
    }
    return 1;
}

Rich4SprSmpError rich4_spr_smp_get_chunk(const unsigned char *data,
                                         size_t size,
                                         const Rich4SprSmpInfo *info,
                                         size_t index,
                                         Rich4SprSmpChunk *out_chunk)
{
    size_t table_offset = RICH4_SPR_SMP_HEADER_SIZE;
    size_t chunk_offset;
    uint32_t graph_offset;
    size_t i;

    if (data == NULL || info == NULL || out_chunk == NULL ||
        size < RICH4_SPR_SMP_HEADER_SIZE ||
        !(memcmp(info->signature, "SPR", 4) == 0 ||
          memcmp(info->signature, "SMP", 4) == 0)) {
        return RICH4_SPR_SMP_ERR_FORMAT;
    }
    if (index >= info->num_chunks) {
        return RICH4_SPR_SMP_ERR_RANGE;
    }
    if ((size_t)info->num_chunks >
        (size - table_offset) / RICH4_SPR_SMP_CHUNK_ENTRY_SIZE) {
        return RICH4_SPR_SMP_ERR_RANGE;
    }
    if (memcmp(info->signature, "SPR", 4) == 0) {
        if (info->start_offset > UINT32_MAX - RICH4_SPR_PALETTE_SIZE) {
            return RICH4_SPR_SMP_ERR_RANGE;
        }
        graph_offset = info->start_offset + RICH4_SPR_PALETTE_SIZE;
    } else {
        graph_offset = info->start_offset;
    }
    for (i = 0; i <= index; i++) {
        uint32_t graph_size;
        chunk_offset = table_offset + i * RICH4_SPR_SMP_CHUNK_ENTRY_SIZE;
        if (!rich4_byte_reader_le32(data, size, chunk_offset + 8,
                                    &graph_size)) {
            return RICH4_SPR_SMP_ERR_RANGE;
        }
        if (i == index) {
            uint16_t value;
            if (!rich4_byte_reader_le16(data, size, chunk_offset, &value))
                return RICH4_SPR_SMP_ERR_RANGE;
            out_chunk->width = (int16_t)value;
            if (!rich4_byte_reader_le16(data, size, chunk_offset + 2, &value))
                return RICH4_SPR_SMP_ERR_RANGE;
            out_chunk->height = (int16_t)value;
            if (!rich4_byte_reader_le16(data, size, chunk_offset + 4, &value))
                return RICH4_SPR_SMP_ERR_RANGE;
            out_chunk->x = (int16_t)value;
            if (!rich4_byte_reader_le16(data, size, chunk_offset + 6, &value))
                return RICH4_SPR_SMP_ERR_RANGE;
            out_chunk->y = (int16_t)value;
            out_chunk->data_size = graph_size;
            out_chunk->data_offset = graph_offset;
            if ((size_t)graph_offset > size ||
                (size_t)graph_size > size - (size_t)graph_offset) {
                return RICH4_SPR_SMP_ERR_RANGE;
            }
            return RICH4_SPR_SMP_OK;
        }
        if (graph_offset > UINT32_MAX - graph_size) {
            return RICH4_SPR_SMP_ERR_RANGE;
        }
        graph_offset += graph_size;
    }
    return RICH4_SPR_SMP_ERR_RANGE;
}

Rich4SprSmpError rich4_spr_smp_decode_rgb555(const unsigned char *data,
                                             size_t size,
                                             const Rich4SprSmpInfo *info,
                                             const Rich4SprSmpChunk *chunk,
                                             uint16_t *out_pixels,
                                             size_t pixel_capacity)
{
    size_t pixel_count;
    size_t i;

    if (data == NULL || info == NULL || chunk == NULL || out_pixels == NULL ||
        chunk->width <= 0 || chunk->height <= 0) {
        return RICH4_SPR_SMP_ERR_FORMAT;
    }
    if ((size_t)chunk->width > SIZE_MAX / (size_t)chunk->height) {
        return RICH4_SPR_SMP_ERR_RANGE;
    }
    pixel_count = (size_t)chunk->width * (size_t)chunk->height;
    if (pixel_capacity < pixel_count || (size_t)chunk->data_offset > size) {
        return RICH4_SPR_SMP_ERR_RANGE;
    }
    if (memcmp(info->signature, "SMP", 4) == 0) {
        if (pixel_count > SIZE_MAX / 2 ||
            (size_t)chunk->data_size < pixel_count * 2 ||
            pixel_count * 2 > size - (size_t)chunk->data_offset) {
            return RICH4_SPR_SMP_ERR_RANGE;
        }
        for (i = 0; i < pixel_count; i++) {
            if (!rich4_byte_reader_le16(data, size,
                                        (size_t)chunk->data_offset + i * 2,
                                        &out_pixels[i]))
                return RICH4_SPR_SMP_ERR_RANGE;
        }
        return RICH4_SPR_SMP_OK;
    }
    if (memcmp(info->signature, "SPR", 4) == 0) {
        if ((size_t)info->start_offset > size ||
            RICH4_SPR_PALETTE_SIZE > size - (size_t)info->start_offset ||
            (size_t)chunk->data_size < pixel_count ||
            pixel_count > size - (size_t)chunk->data_offset) {
            return RICH4_SPR_SMP_ERR_RANGE;
        }
        for (i = 0; i < pixel_count; i++) {
            unsigned int palette_index = data[chunk->data_offset + i];
            if (!rich4_byte_reader_le16(data, size,
                                        (size_t)info->start_offset + palette_index * 2,
                                        &out_pixels[i]))
                return RICH4_SPR_SMP_ERR_RANGE;
        }
        return RICH4_SPR_SMP_OK;
    }
    return RICH4_SPR_SMP_ERR_FORMAT;
}

const char *rich4_spr_smp_error_name(Rich4SprSmpError error)
{
    switch (error) {
    case RICH4_SPR_SMP_OK: return "ok";
    case RICH4_SPR_SMP_ERR_FORMAT: return "format";
    case RICH4_SPR_SMP_ERR_RANGE: return "range";
    default: return "unknown";
    }
}
