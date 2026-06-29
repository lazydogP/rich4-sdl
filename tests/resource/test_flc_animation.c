#include "resource/flc_animation.h"

#include <stdio.h>
#include <string.h>

static void put_u16(unsigned char *p, unsigned int value)
{
    p[0] = (unsigned char)(value & 0xffu);
    p[1] = (unsigned char)((value >> 8) & 0xffu);
}

static void put_u32(unsigned char *p, uint32_t value)
{
    p[0] = (unsigned char)(value & 0xffu);
    p[1] = (unsigned char)((value >> 8) & 0xffu);
    p[2] = (unsigned char)((value >> 16) & 0xffu);
    p[3] = (unsigned char)((value >> 24) & 0xffu);
}

static size_t make_one_chunk_flc(unsigned char *data, size_t capacity,
                                 int width, int height, int delay,
                                 unsigned int opcode,
                                 const unsigned char *payload,
                                 size_t payload_size)
{
    size_t chunk_size = RICH4_FLC_CHUNK_HEADER_SIZE + payload_size;
    size_t frame_size = RICH4_FLC_FRAME_HEADER_SIZE + chunk_size;
    size_t total = RICH4_FLC_HEADER_SIZE + frame_size;
    if (capacity < total) return 0;
    memset(data, 0, total);
    put_u32(data, (uint32_t)total);
    put_u16(data + 4, RICH4_FLC_MAGIC);
    put_u16(data + 6, 1);
    put_u16(data + 8, (unsigned int)width);
    put_u16(data + 10, (unsigned int)height);
    put_u16(data + 12, 8);
    put_u32(data + 16, (uint32_t)delay);
    put_u32(data + RICH4_FLC_HEADER_SIZE, (uint32_t)frame_size);
    put_u16(data + RICH4_FLC_HEADER_SIZE + 4, RICH4_FLC_FRAME_MAGIC);
    put_u16(data + RICH4_FLC_HEADER_SIZE + 6, 1);
    put_u32(data + RICH4_FLC_HEADER_SIZE + RICH4_FLC_FRAME_HEADER_SIZE,
            (uint32_t)chunk_size);
    put_u16(data + RICH4_FLC_HEADER_SIZE + RICH4_FLC_FRAME_HEADER_SIZE + 4,
            opcode);
    if (payload_size != 0) {
        memcpy(data + RICH4_FLC_HEADER_SIZE + RICH4_FLC_FRAME_HEADER_SIZE +
                   RICH4_FLC_CHUNK_HEADER_SIZE,
               payload, payload_size);
    }
    return total;
}

static size_t append_chunk(unsigned char *data, size_t capacity, size_t total,
                           unsigned int opcode,
                           const unsigned char *payload, size_t payload_size)
{
    size_t chunk_size = RICH4_FLC_CHUNK_HEADER_SIZE + payload_size;
    size_t frame_offset = RICH4_FLC_HEADER_SIZE;
    if (chunk_size > capacity - total) return 0;
    put_u32(data + total, (uint32_t)chunk_size);
    put_u16(data + total + 4, opcode);
    if (payload_size != 0) memcpy(data + total + 6, payload, payload_size);
    total += chunk_size;
    put_u32(data, (uint32_t)total);
    put_u32(data + frame_offset, (uint32_t)(total - frame_offset));
    put_u16(data + frame_offset + 6, 2);
    return total;
}

static int init_and_decode(unsigned char *data, size_t size,
                           uint16_t *pixels, size_t pixel_count,
                           Rich4FlcDecoder *decoder)
{
    if (rich4_flc_decoder_init(decoder, data, size, pixels, pixel_count) !=
        RICH4_FLC_OK) return 0;
    decoder->palette[1] = 0x1111u;
    decoder->palette[2] = 0x2222u;
    decoder->palette[3] = 0x3333u;
    return rich4_flc_decode_next(decoder, NULL) == RICH4_FLC_OK;
}

int main(void)
{
    unsigned char data[512];
    uint16_t pixels[8];
    Rich4FlcHeader header;
    Rich4FlcDecoder decoder;
    Rich4FlcFrame frame;
    size_t size;

    static const unsigned char palette_payload[] = {
        1, 0, 1, 1, 255, 0, 0
    };
    static const unsigned char palette6_payload[] = {
        1, 0, 1, 1, 63, 0, 0
    };
    static const unsigned char rle7_payload[] = {
        1, 0, 1, 0, 0, 1, 1, 2
    };
    static const unsigned char rle12_payload[] = {
        0, 0, 1, 0, 1, 0, 3, 1, 2, 3
    };
    static const unsigned char rle15_payload[] = {1, 3, 2};
    static const unsigned char raw16_payload[] = {1, 2, 3};

    size = make_one_chunk_flc(data, sizeof(data), 2, 1, 77, 4,
                              palette_payload, sizeof(palette_payload));
    if (rich4_flc_parse_header(data, size, &header) != RICH4_FLC_OK ||
        header.frame_count != 1 || header.width != 2 || header.height != 1 ||
        header.file_size != size || header.depth != 8 || header.delay_ms != 77 ||
        rich4_flc_decoder_init(&decoder, data, size, pixels, 8) != RICH4_FLC_OK ||
        rich4_flc_decode_next(&decoder, &frame) != RICH4_FLC_OK ||
        frame.offset != RICH4_FLC_HEADER_SIZE || frame.command_count != 1 ||
        frame.size != RICH4_FLC_FRAME_HEADER_SIZE +
                          RICH4_FLC_CHUNK_HEADER_SIZE + sizeof(palette_payload) ||
        decoder.palette[1] != 0xf800u ||
        rich4_flc_decode_next(&decoder, NULL) != RICH4_FLC_DONE) {
        fprintf(stderr, "FLC header/opcode 4 case failed\n");
        return 1;
    }

    size = make_one_chunk_flc(data, sizeof(data), 2, 1, 10, 7,
                              rle7_payload, sizeof(rle7_payload));
    if (!init_and_decode(data, size, pixels, 8, &decoder) ||
        pixels[0] != 0x1111u || pixels[1] != 0x2222u) {
        fprintf(stderr, "FLC opcode 7 case failed\n");
        return 1;
    }

    {
        uint16_t source[4] = {0xaaaa, 0xbbbbu, 0xccccu, 0xddddu};
        size = make_one_chunk_flc(data, sizeof(data), 4, 1, 10, 7,
                                  rle7_payload, sizeof(rle7_payload));
        if (rich4_flc_decoder_init(&decoder, data, size, pixels, 8) !=
                RICH4_FLC_OK) {
            fprintf(stderr, "FLC source framebuffer init failed\n");
            return 1;
        }
        decoder.palette[1] = 0x1111u;
        decoder.palette[2] = 0x2222u;
        rich4_flc_decoder_set_source(&decoder, source, 4);
        if (rich4_flc_decode_next(&decoder, NULL) != RICH4_FLC_OK ||
            pixels[0] != 0x1111u || pixels[1] != 0x2222u ||
            pixels[2] != 0xccccu || pixels[3] != 0xddddu) {
            fprintf(stderr, "FLC source framebuffer delta case failed\n");
            return 1;
        }
    }

    size = make_one_chunk_flc(data, sizeof(data), 1, 1, 10, 11,
                              palette6_payload, sizeof(palette6_payload));
    if (rich4_flc_decoder_init(&decoder, data, size, pixels, 8) != RICH4_FLC_OK ||
        rich4_flc_decode_next(&decoder, NULL) != RICH4_FLC_OK ||
        decoder.palette[1] != 0xf800u) {
        fprintf(stderr, "FLC opcode 11 palette case failed\n");
        return 1;
    }

    size = make_one_chunk_flc(data, sizeof(data), 2, 1, 10, 13, NULL, 0);
    pixels[0] = 0x1111u;
    pixels[1] = 0x2222u;
    if (rich4_flc_decoder_init(&decoder, data, size, pixels, 8) != RICH4_FLC_OK) return 1;
    decoder.palette[0] = 0x3333u;
    {
        unsigned char dirty[2] = {0, 0};
        rich4_flc_decoder_set_dirty(&decoder, dirty, 2);
        if (rich4_flc_decode_next(&decoder, NULL) != RICH4_FLC_OK ||
            pixels[0] != 0x3333u || pixels[1] != 0x3333u ||
            dirty[0] != 1 || dirty[1] != 1) {
            fprintf(stderr, "FLC opcode 13 black case failed\n");
            return 1;
        }
    }

    size = make_one_chunk_flc(data, sizeof(data), 3, 1, 10, 12,
                              rle12_payload, sizeof(rle12_payload));
    if (!init_and_decode(data, size, pixels, 8, &decoder) ||
        pixels[0] != 0x1111u || pixels[1] != 0x2222u || pixels[2] != 0x3333u) {
        fprintf(stderr, "FLC opcode 12 case failed\n");
        return 1;
    }

    size = make_one_chunk_flc(data, sizeof(data), 3, 1, 10, 15,
                              rle15_payload, sizeof(rle15_payload));
    if (!init_and_decode(data, size, pixels, 8, &decoder) ||
        pixels[0] != 0x2222u || pixels[1] != 0x2222u || pixels[2] != 0x2222u) {
        fprintf(stderr, "FLC opcode 15 case failed\n");
        return 1;
    }

    size = make_one_chunk_flc(data, sizeof(data), 3, 1, 10, 16,
                              raw16_payload, sizeof(raw16_payload));
    if (!init_and_decode(data, size, pixels, 8, &decoder) ||
        pixels[0] != 0x1111u || pixels[1] != 0x2222u || pixels[2] != 0x3333u) {
        fprintf(stderr, "FLC opcode 16 case failed\n");
        return 1;
    }

    size = make_one_chunk_flc(data, sizeof(data), 3, 1, 10, 18, NULL, 0);
    size = append_chunk(data, sizeof(data), size, 16,
                        raw16_payload, sizeof(raw16_payload));
    if (size == 0 || !init_and_decode(data, size, pixels, 8, &decoder) ||
        pixels[0] != 0x1111u || pixels[1] != 0x2222u || pixels[2] != 0x3333u ||
        decoder.skipped_chunk_count != 1 || decoder.last_skipped_opcode != 18) {
        fprintf(stderr, "FLC auxiliary opcode skip case failed\n");
        return 1;
    }

    size = make_one_chunk_flc(data, sizeof(data), 1, 1, 10, 99, NULL, 0);
    if (rich4_flc_decoder_init(&decoder, data, size, pixels, 8) != RICH4_FLC_OK ||
        rich4_flc_decode_next(&decoder, NULL) != RICH4_FLC_ERR_UNSUPPORTED_OPCODE) {
        fprintf(stderr, "FLC unsupported opcode case failed\n");
        return 1;
    }
    if (rich4_flc_parse_header(data, RICH4_FLC_HEADER_SIZE - 1, &header) !=
            RICH4_FLC_ERR_TRUNCATED) {
        fprintf(stderr, "FLC truncated header case failed\n");
        return 1;
    }
    data[4] = 0;
    data[5] = 0;
    if (rich4_flc_parse_header(data, size, &header) != RICH4_FLC_ERR_MAGIC) {
        fprintf(stderr, "FLC bad magic case failed\n");
        return 1;
    }
    put_u16(data + 4, RICH4_FLC_MAGIC);
    put_u32(data + RICH4_FLC_HEADER_SIZE + RICH4_FLC_FRAME_HEADER_SIZE, 1000);
    if (rich4_flc_decoder_init(&decoder, data, size, pixels, 8) != RICH4_FLC_OK ||
        rich4_flc_decode_next(&decoder, NULL) != RICH4_FLC_ERR_TRUNCATED) {
        fprintf(stderr, "FLC truncated chunk case failed\n");
        return 1;
    }

    puts("Autodesk Animator FLC decoder: ok");
    return 0;
}
