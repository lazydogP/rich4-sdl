#include "resource/flc_animation.h"

#include <limits.h>
#include <string.h>

#include "resource/byte_reader.h"

static uint16_t rgb888_to_rgb565(const unsigned char *rgb)
{
    uint16_t r = (uint16_t)(rgb[0] >> 3);
    uint16_t g = (uint16_t)(rgb[1] >> 2);
    uint16_t b = (uint16_t)(rgb[2] >> 3);
    return (uint16_t)((r << 11) | (g << 5) | b);
}

static size_t decoder_pixel_count(const Rich4FlcDecoder *decoder)
{
    return (size_t)decoder->header.width * (size_t)decoder->header.height;
}

static void put_index(Rich4FlcDecoder *decoder, int x, int y,
                      unsigned int palette_index)
{
    size_t position;
    if (x < 0 || y < 0 || x >= decoder->header.width ||
        y >= decoder->header.height) {
        return;
    }
    position = (size_t)y * decoder->header.width + (size_t)x;
    if (decoder->index_pixels != NULL && position < decoder->index_capacity)
        decoder->index_pixels[position] = (unsigned char)(palette_index & 0xffu);
    decoder->pixels[position] = decoder->palette[palette_index & 0xffu];
    if (decoder->dirty_pixels != NULL && position < decoder->dirty_capacity) {
        decoder->dirty_pixels[position] = 1;
    }
}

static Rich4FlcError decode_palette(Rich4FlcDecoder *decoder,
                                    const unsigned char *payload,
                                    size_t payload_size,
                                    int six_bit)
{
    size_t offset = 2;
    uint16_t groups;
    uint16_t i;
    if (payload_size < 2) return RICH4_FLC_ERR_TRUNCATED;
    if (!rich4_byte_reader_le16(payload, payload_size, 0, &groups))
        return RICH4_FLC_ERR_TRUNCATED;
    for (i = 0; i < groups; i++) {
        unsigned int start;
        unsigned int count;
        unsigned int j;
        if (offset + 2 > payload_size) return RICH4_FLC_ERR_TRUNCATED;
        start = payload[offset++];
        count = payload[offset++];
        if (count == 0) count = 256;
        if ((size_t)count > (payload_size - offset) / 3) {
            return RICH4_FLC_ERR_TRUNCATED;
        }
        for (j = 0; j < count; j++) {
            if (start + j < 256) {
                if (six_bit) {
                    unsigned char rgb[3];
                    rgb[0] = (unsigned char)(payload[offset] << 2);
                    rgb[1] = (unsigned char)(payload[offset + 1] << 2);
                    rgb[2] = (unsigned char)(payload[offset + 2] << 2);
                    decoder->palette[start + j] = rgb888_to_rgb565(rgb);
                } else {
                    decoder->palette[start + j] = rgb888_to_rgb565(payload + offset);
                }
            }
            offset += 3;
        }
    }
    return RICH4_FLC_OK;
}

static Rich4FlcError decode_black(Rich4FlcDecoder *decoder,
                                  const unsigned char *payload,
                                  size_t payload_size)
{
    size_t count = decoder_pixel_count(decoder);
    size_t i;
    (void)payload;
    if (payload_size != 0) return RICH4_FLC_ERR_RANGE;
    for (i = 0; i < count; i++) {
        put_index(decoder,
                  (int)(i % decoder->header.width),
                  (int)(i / decoder->header.width),
                  0);
    }
    return RICH4_FLC_OK;
}

static Rich4FlcError decode_rle7(Rich4FlcDecoder *decoder,
                                 const unsigned char *payload,
                                 size_t payload_size)
{
    size_t offset = 2;
    uint16_t row_count;
    int rows_done = 0;
    int row = 0;
    if (payload_size < 2) return RICH4_FLC_ERR_TRUNCATED;
    if (!rich4_byte_reader_le16(payload, payload_size, 0, &row_count))
        return RICH4_FLC_ERR_TRUNCATED;
    while (rows_done < (int)row_count) {
        uint16_t row_header;
        int x = 0;
        if (offset + 2 > payload_size) return RICH4_FLC_ERR_TRUNCATED;
        if (!rich4_byte_reader_le16(payload, payload_size, offset, &row_header))
            return RICH4_FLC_ERR_TRUNCATED;
        offset += 2;
        while ((row_header & 0xc000u) == 0xc000u) {
            row += (uint16_t)(0u - row_header);
            if (offset + 2 > payload_size) return RICH4_FLC_ERR_TRUNCATED;
            if (!rich4_byte_reader_le16(payload, payload_size, offset,
                                        &row_header))
                return RICH4_FLC_ERR_TRUNCATED;
            offset += 2;
        }
        if (row_header & 0x8000u) {
            row++;
            rows_done++;
            continue;
        }
        while (row_header-- > 0) {
            unsigned int skip;
            unsigned int count;
            if (offset + 2 > payload_size) return RICH4_FLC_ERR_TRUNCATED;
            skip = payload[offset++];
            count = payload[offset++];
            x += (int)skip;
            if (count < 0x80u) {
                unsigned int literal_count = count * 2;
                unsigned int i;
                if ((size_t)literal_count > payload_size - offset) {
                    return RICH4_FLC_ERR_TRUNCATED;
                }
                for (i = 0; i < literal_count; i++) {
                    put_index(decoder, x++, row, payload[offset++]);
                }
            } else {
                unsigned int repeats = (unsigned char)(0u - count);
                unsigned int index0;
                unsigned int index1;
                unsigned int i;
                if (offset + 2 > payload_size) return RICH4_FLC_ERR_TRUNCATED;
                index0 = payload[offset++];
                index1 = payload[offset++];
                for (i = 0; i < repeats; i++) {
                    put_index(decoder, x++, row, index0);
                    put_index(decoder, x++, row, index1);
                }
            }
        }
        row++;
        rows_done++;
    }
    return RICH4_FLC_OK;
}

static Rich4FlcError decode_rle12(Rich4FlcDecoder *decoder,
                                  const unsigned char *payload,
                                  size_t payload_size)
{
    size_t offset = 4;
    int y_offset;
    int row_count;
    int row;
    if (payload_size < 4) return RICH4_FLC_ERR_TRUNCATED;
    {
        uint16_t value;
        if (!rich4_byte_reader_le16(payload, payload_size, 0, &value))
            return RICH4_FLC_ERR_TRUNCATED;
        y_offset = value;
        if (!rich4_byte_reader_le16(payload, payload_size, 2, &value))
            return RICH4_FLC_ERR_TRUNCATED;
        row_count = value;
    }
    for (row = 0; row < row_count; row++) {
        int x = 0;
        unsigned int segment_count;
        unsigned int segment;
        if (offset >= payload_size) return RICH4_FLC_ERR_TRUNCATED;
        segment_count = payload[offset++];
        for (segment = 0; segment < segment_count; segment++) {
            unsigned int skip;
            unsigned int count;
            unsigned int i;
            if (offset + 2 > payload_size) return RICH4_FLC_ERR_TRUNCATED;
            skip = payload[offset++];
            count = payload[offset++];
            x += (int)skip;
            if (count >= 0x80u) {
                unsigned int index;
                count = (unsigned char)(0u - count);
                if (offset >= payload_size) return RICH4_FLC_ERR_TRUNCATED;
                index = payload[offset++];
                for (i = 0; i < count; i++) {
                    put_index(decoder, x++, y_offset + row, index);
                }
            } else {
                if ((size_t)count > payload_size - offset) {
                    return RICH4_FLC_ERR_TRUNCATED;
                }
                for (i = 0; i < count; i++) {
                    put_index(decoder, x++, y_offset + row, payload[offset++]);
                }
            }
        }
    }
    return RICH4_FLC_OK;
}

static Rich4FlcError decode_rle15(Rich4FlcDecoder *decoder,
                                  const unsigned char *payload,
                                  size_t payload_size)
{
    size_t offset = 0;
    int row;
    for (row = 0; row < decoder->header.height; row++) {
        int x = 0;
        if (offset >= payload_size) return RICH4_FLC_ERR_TRUNCATED;
        offset++;
        while (x < decoder->header.width) {
            unsigned int code;
            unsigned int count;
            unsigned int i;
            if (offset >= payload_size) return RICH4_FLC_ERR_TRUNCATED;
            code = payload[offset++];
            if (code <= 0x80u) {
                unsigned int index;
                count = code;
                if (offset >= payload_size) return RICH4_FLC_ERR_TRUNCATED;
                index = payload[offset++];
                for (i = 0; i < count && x < decoder->header.width; i++) {
                    put_index(decoder, x++, row, index);
                }
            } else {
                count = (unsigned char)(0u - code);
                if ((size_t)count > payload_size - offset) {
                    return RICH4_FLC_ERR_TRUNCATED;
                }
                for (i = 0; i < count && x < decoder->header.width; i++) {
                    put_index(decoder, x++, row, payload[offset++]);
                }
            }
        }
    }
    return RICH4_FLC_OK;
}

static Rich4FlcError decode_raw16(Rich4FlcDecoder *decoder,
                                  const unsigned char *payload,
                                  size_t payload_size)
{
    size_t count = decoder_pixel_count(decoder);
    size_t i;
    if (payload_size < count) return RICH4_FLC_ERR_TRUNCATED;
    for (i = 0; i < count; i++) put_index(decoder, (int)(i % decoder->header.width),
                                          (int)(i / decoder->header.width), payload[i]);
    return RICH4_FLC_OK;
}

static Rich4FlcError decode_chunk(Rich4FlcDecoder *decoder, uint16_t opcode,
                                  const unsigned char *payload,
                                  size_t payload_size,
                                  int decode_pixels)
{
    if (!decode_pixels && opcode != 4 && opcode != 11) {
        if (opcode == 7 || opcode == 12 || opcode == 13 || opcode == 15 ||
            opcode == 16 || opcode == 18) {
            return RICH4_FLC_OK;
        }
        return RICH4_FLC_ERR_UNSUPPORTED_OPCODE;
    }
    switch (opcode) {
    case 4: return decode_palette(decoder, payload, payload_size, 0);
    case 7: return decode_rle7(decoder, payload, payload_size);
    case 11: return decode_palette(decoder, payload, payload_size, 1);
    case 12: return decode_rle12(decoder, payload, payload_size);
    case 13: return decode_black(decoder, payload, payload_size);
    case 15: return decode_rle15(decoder, payload, payload_size);
    case 16: return decode_raw16(decoder, payload, payload_size);
    case 18: return RICH4_FLC_OK; /* postage-stamp/preview data is non-display metadata */
    default: return RICH4_FLC_ERR_UNSUPPORTED_OPCODE;
    }
}

Rich4FlcError rich4_flc_parse_header(const unsigned char *data, size_t size,
                                     Rich4FlcHeader *out_header)
{
    size_t pixel_count;
    uint16_t value16;
    uint32_t value32;
    if (data == NULL || out_header == NULL) return RICH4_FLC_ERR_ARGUMENT;
    if (size < RICH4_FLC_HEADER_SIZE) return RICH4_FLC_ERR_TRUNCATED;
    if (!rich4_byte_reader_le16(data, size, 4, &value16))
        return RICH4_FLC_ERR_TRUNCATED;
    if (value16 != RICH4_FLC_MAGIC) return RICH4_FLC_ERR_MAGIC;
    if (!rich4_byte_reader_le32(data, size, 0, &value32))
        return RICH4_FLC_ERR_TRUNCATED;
    out_header->file_size = value32;
    if (!rich4_byte_reader_le16(data, size, 6, &value16))
        return RICH4_FLC_ERR_TRUNCATED;
    out_header->frame_count = value16;
    if (!rich4_byte_reader_le16(data, size, 8, &value16))
        return RICH4_FLC_ERR_TRUNCATED;
    out_header->width = value16;
    if (!rich4_byte_reader_le16(data, size, 10, &value16))
        return RICH4_FLC_ERR_TRUNCATED;
    out_header->height = value16;
    if (!rich4_byte_reader_le16(data, size, 12, &value16))
        return RICH4_FLC_ERR_TRUNCATED;
    out_header->depth = value16;
    if (!rich4_byte_reader_le16(data, size, 14, &value16))
        return RICH4_FLC_ERR_TRUNCATED;
    out_header->flags = value16;
    if (!rich4_byte_reader_le32(data, size, 16, &value32))
        return RICH4_FLC_ERR_TRUNCATED;
    out_header->delay_ms = value32;
    if (out_header->width == 0 || out_header->height == 0 ||
        (size_t)out_header->width > SIZE_MAX / (size_t)out_header->height) {
        return RICH4_FLC_ERR_RANGE;
    }
    pixel_count = (size_t)out_header->width * out_header->height;
    if (pixel_count > INT_MAX) return RICH4_FLC_ERR_RANGE;
    return RICH4_FLC_OK;
}

Rich4FlcError rich4_flc_decoder_init(Rich4FlcDecoder *decoder,
                                     const unsigned char *data, size_t size,
                                     uint16_t *pixels, size_t pixel_capacity)
{
    Rich4FlcHeader header;
    Rich4FlcError error;
    size_t count;
    if (decoder == NULL || pixels == NULL) return RICH4_FLC_ERR_ARGUMENT;
    error = rich4_flc_parse_header(data, size, &header);
    if (error != RICH4_FLC_OK) return error;
    count = (size_t)header.width * header.height;
    if (pixel_capacity < count) return RICH4_FLC_ERR_RANGE;
    memset(decoder, 0, sizeof(*decoder));
    decoder->data = data;
    decoder->size = size;
    decoder->header = header;
    decoder->next_frame_offset = RICH4_FLC_HEADER_SIZE;
    decoder->pixels = pixels;
    decoder->pixel_capacity = pixel_capacity;
    memset(pixels, 0, count * sizeof(*pixels));
    return RICH4_FLC_OK;
}

void rich4_flc_decoder_set_source(Rich4FlcDecoder *decoder,
                                  const uint16_t *source_pixels,
                                  size_t source_capacity)
{
    size_t count;
    if (decoder == NULL || decoder->pixels == NULL || source_pixels == NULL)
        return;
    count = decoder_pixel_count(decoder);
    if (count > decoder->pixel_capacity || source_capacity < count) return;
    memmove(decoder->pixels, source_pixels, count * sizeof(*decoder->pixels));
}

void rich4_flc_decoder_set_indices(Rich4FlcDecoder *decoder,
                                   unsigned char *index_pixels,
                                   size_t index_capacity)
{
    size_t count;
    if (decoder == NULL) return;
    count = decoder_pixel_count(decoder);
    decoder->index_pixels = index_pixels;
    decoder->index_capacity = index_capacity;
    if (index_pixels != NULL && index_capacity >= count)
        memset(index_pixels, 0, count);
}

void rich4_flc_decoder_set_dirty(Rich4FlcDecoder *decoder,
                                 unsigned char *dirty_pixels,
                                 size_t dirty_capacity)
{
    if (decoder == NULL) return;
    decoder->dirty_pixels = dirty_pixels;
    decoder->dirty_capacity = dirty_capacity;
    if (dirty_pixels != NULL) memset(dirty_pixels, 0, dirty_capacity);
}

Rich4FlcError rich4_flc_decode_next(Rich4FlcDecoder *decoder,
                                    Rich4FlcFrame *out_frame)
{
    return rich4_flc_decode_next_ex(decoder, out_frame, 1);
}

Rich4FlcError rich4_flc_decode_next_ex(Rich4FlcDecoder *decoder,
                                       Rich4FlcFrame *out_frame,
                                       int decode_pixels)
{
    size_t offset;
    uint32_t frame_size;
    uint16_t command_count;
    size_t command_offset;
    size_t frame_end;
    uint16_t command;
    if (decoder == NULL || decoder->data == NULL || decoder->pixels == NULL) {
        return RICH4_FLC_ERR_ARGUMENT;
    }
    if (decode_pixels && decoder->dirty_pixels != NULL) {
        memset(decoder->dirty_pixels, 0, decoder->dirty_capacity);
    }
    if (decoder->next_frame_index >= decoder->header.frame_count) return RICH4_FLC_DONE;
    offset = decoder->next_frame_offset;
    while (1) {
        if (offset + RICH4_FLC_FRAME_HEADER_SIZE > decoder->size) {
            return RICH4_FLC_ERR_TRUNCATED;
        }
        if (!rich4_byte_reader_le32(decoder->data, decoder->size, offset,
                                    &frame_size))
            return RICH4_FLC_ERR_TRUNCATED;
        if (frame_size < RICH4_FLC_FRAME_HEADER_SIZE ||
            (size_t)frame_size > decoder->size - offset) {
            return RICH4_FLC_ERR_TRUNCATED;
        }
        {
            uint16_t frame_magic;
            if (!rich4_byte_reader_le16(decoder->data, decoder->size,
                                        offset + 4, &frame_magic))
                return RICH4_FLC_ERR_TRUNCATED;
            if (frame_magic == RICH4_FLC_FRAME_MAGIC) break;
        }
        offset += frame_size;
    }
    if (!rich4_byte_reader_le16(decoder->data, decoder->size, offset + 6,
                                &command_count))
        return RICH4_FLC_ERR_TRUNCATED;
    command_offset = offset + RICH4_FLC_FRAME_HEADER_SIZE;
    frame_end = offset + frame_size;
    for (command = 0; command < command_count; command++) {
        uint32_t chunk_size;
        uint16_t opcode;
        Rich4FlcError error;
        if (command_offset + RICH4_FLC_CHUNK_HEADER_SIZE > frame_end) {
            return RICH4_FLC_ERR_TRUNCATED;
        }
        if (!rich4_byte_reader_le32(decoder->data, decoder->size,
                                    command_offset, &chunk_size) ||
            !rich4_byte_reader_le16(decoder->data, decoder->size,
                                    command_offset + 4, &opcode))
            return RICH4_FLC_ERR_TRUNCATED;
        if (chunk_size < RICH4_FLC_CHUNK_HEADER_SIZE ||
            (size_t)chunk_size > frame_end - command_offset) {
            return RICH4_FLC_ERR_TRUNCATED;
        }
        if (opcode == 18) {
            decoder->last_skipped_opcode = opcode;
            decoder->skipped_chunk_count++;
        }
        error = decode_chunk(decoder, opcode,
                             decoder->data + command_offset + RICH4_FLC_CHUNK_HEADER_SIZE,
                             chunk_size - RICH4_FLC_CHUNK_HEADER_SIZE,
                             decode_pixels);
        if (error != RICH4_FLC_OK) return error;
        command_offset += chunk_size;
    }
    if (decode_pixels && decoder->index_pixels != NULL &&
        decoder->index_capacity >= decoder_pixel_count(decoder)) {
        size_t i;
        size_t count = decoder_pixel_count(decoder);
        for (i = 0; i < count; i++)
            decoder->pixels[i] = decoder->palette[decoder->index_pixels[i]];
    }
    if (out_frame != NULL) {
        out_frame->offset = offset;
        out_frame->size = frame_size;
        out_frame->command_count = command_count;
    }
    decoder->next_frame_offset = frame_end;
    decoder->next_frame_index++;
    return RICH4_FLC_OK;
}

const char *rich4_flc_error_name(Rich4FlcError error)
{
    switch (error) {
    case RICH4_FLC_OK: return "ok";
    case RICH4_FLC_DONE: return "done";
    case RICH4_FLC_ERR_ARGUMENT: return "argument";
    case RICH4_FLC_ERR_MAGIC: return "magic";
    case RICH4_FLC_ERR_TRUNCATED: return "truncated";
    case RICH4_FLC_ERR_RANGE: return "range";
    case RICH4_FLC_ERR_UNSUPPORTED_OPCODE: return "unsupported-opcode";
    default: return "unknown";
    }
}
