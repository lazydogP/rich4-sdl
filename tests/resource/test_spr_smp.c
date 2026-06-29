#include "resource/spr_smp.h"

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

static void make_smp(unsigned char data[28])
{
    memset(data, 0, 28);
    memcpy(data, "SMP", 3);
    put_u32(data + 4, 1);
    put_u32(data + 8, 24);
    put_u16(data + 12, 2);
    put_u16(data + 14, 2);
    put_u16(data + 16, 1);
    put_u16(data + 18, 0xffffu);
    put_u32(data + 20, 4);
    data[24] = 1;
    data[25] = 2;
    data[26] = 3;
    data[27] = 4;
}

int main(void)
{
    unsigned char data[28];
    unsigned char decode_smp[28];
    unsigned char decode_spr[538];
    unsigned char bad_header[12] = {0};
    Rich4SprSmpInfo info;
    Rich4SprSmpChunk chunk;
    uint16_t pixels[4] = {0, 0, 0, 0};

    if (rich4_spr_smp_probe(NULL, 0, &info) ||
        rich4_spr_smp_probe(bad_header, 0, &info) ||
        rich4_spr_smp_probe(bad_header, sizeof(bad_header), &info) ||
        rich4_spr_smp_get_chunk(NULL, 0, &info, 0, &chunk) !=
            RICH4_SPR_SMP_ERR_FORMAT) {
        fprintf(stderr, "zero-length/corrupt header case failed\n");
        return 1;
    }

    make_smp(data);
    if (!rich4_spr_smp_probe(data, sizeof(data), &info) ||
        info.num_chunks != 1 || info.start_offset != 24 ||
        rich4_spr_smp_get_chunk(data, sizeof(data), &info, 0, &chunk) !=
            RICH4_SPR_SMP_OK ||
        chunk.width != 2 || chunk.height != 2 || chunk.x != 1 || chunk.y != -1 ||
        chunk.data_offset != 24 || chunk.data_size != 4) {
        fprintf(stderr, "valid SMP case failed\n");
        return 1;
    }

    make_smp(decode_smp);
    put_u16(decode_smp + 14, 1);
    put_u32(decode_smp + 20, 4);
    decode_smp[24] = 0x34;
    decode_smp[25] = 0x12;
    decode_smp[26] = 0xcd;
    decode_smp[27] = 0xab;
    if (!rich4_spr_smp_probe(decode_smp, sizeof(decode_smp), &info) ||
        rich4_spr_smp_get_chunk(decode_smp, sizeof(decode_smp), &info, 0, &chunk) !=
            RICH4_SPR_SMP_OK ||
        rich4_spr_smp_decode_rgb555(decode_smp, sizeof(decode_smp), &info,
                                    &chunk, pixels, 4) != RICH4_SPR_SMP_OK ||
        pixels[0] != 0x1234u || pixels[1] != 0xabcdu) {
        fprintf(stderr, "SMP decode case failed\n");
        return 1;
    }

    memset(decode_spr, 0, sizeof(decode_spr));
    memcpy(decode_spr, "SPR", 3);
    put_u32(decode_spr + 4, 1);
    put_u32(decode_spr + 8, 24);
    put_u16(decode_spr + 12, 2);
    put_u16(decode_spr + 14, 1);
    put_u32(decode_spr + 20, 2);
    put_u16(decode_spr + 24 + 2, 0x4567u);
    decode_spr[536] = 1;
    decode_spr[537] = 1;
    if (!rich4_spr_smp_probe(decode_spr, sizeof(decode_spr), &info) ||
        rich4_spr_smp_get_chunk(decode_spr, sizeof(decode_spr), &info, 0, &chunk) !=
            RICH4_SPR_SMP_OK ||
        rich4_spr_smp_decode_rgb555(decode_spr, sizeof(decode_spr), &info,
                                    &chunk, pixels, 4) != RICH4_SPR_SMP_OK ||
        pixels[0] != 0x4567u || pixels[1] != 0x4567u) {
        fprintf(stderr, "SPR decode case failed\n");
        return 1;
    }
    if (rich4_spr_smp_get_chunk(data, sizeof(data), &info, 1, &chunk) !=
        RICH4_SPR_SMP_ERR_RANGE) {
        fprintf(stderr, "chunk index range case failed\n");
        return 1;
    }

    put_u32(data + 20, 5);
    if (rich4_spr_smp_get_chunk(data, sizeof(data), &info, 0, &chunk) !=
        RICH4_SPR_SMP_ERR_RANGE) {
        fprintf(stderr, "chunk data overflow case failed\n");
        return 1;
    }

    make_smp(data);
    put_u32(data + 4, 2);
    if (rich4_spr_smp_probe(data, sizeof(data), &info)) {
        fprintf(stderr, "truncated chunk table case failed\n");
        return 1;
    }

    make_smp(data);
    put_u32(data + 8, 29);
    if (!rich4_spr_smp_probe(data, sizeof(data), &info) ||
        rich4_spr_smp_get_chunk(data, sizeof(data), &info, 0, &chunk) !=
            RICH4_SPR_SMP_ERR_RANGE) {
        fprintf(stderr, "chunk start overflow case failed\n");
        return 1;
    }

    puts("SPR/SMP parser: ok");
    return 0;
}
