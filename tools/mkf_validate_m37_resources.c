#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "resource/flc_animation.h"
#include "resource/mkf.h"
#include "resource/spr_smp.h"

static int read_resource(Rich4Mkf *mkf, size_t index,
                         unsigned char **out_data, size_t *out_size,
                         const char *archive, const char *kind)
{
    Rich4MkfError error;

    error = rich4_mkf_read_resource(mkf, index, out_data, out_size, NULL);
    if (error != RICH4_MKF_OK) {
        fprintf(stderr, "%s[%lu] %s read failed: %s\n", archive,
                (unsigned long)index, kind, rich4_mkf_error_name(error));
        return 0;
    }
    if (*out_data == NULL || *out_size == 0) {
        fprintf(stderr, "%s[%lu] %s is empty\n", archive,
                (unsigned long)index, kind);
        free(*out_data);
        *out_data = NULL;
        *out_size = 0;
        return 0;
    }
    return 1;
}

static int validate_spr_resource(Rich4Mkf *mkf, size_t index,
                                 unsigned int expected_chunks,
                                 int directional,
                                 const char *archive, const char *kind,
                                 uint32_t expected_pixel_digest,
                                 const uint32_t *expected_chunk_digests)
{
    unsigned char *data = NULL;
    size_t size = 0;
    Rich4SprSmpInfo info;
    unsigned int chunk_index;
    uint32_t pixel_digest = 2166136261u;
    int ok = 1;

    if (!read_resource(mkf, index, &data, &size, archive, kind)) return 0;
    if (!rich4_spr_smp_probe(data, size, &info)) {
        fprintf(stderr, "%s[%lu] %s is not SPR/SMP\n", archive,
                (unsigned long)index, kind);
        free(data);
        return 0;
    }
    if (expected_chunks != 0 && info.num_chunks != expected_chunks) {
        fprintf(stderr, "%s[%lu] %s chunk count %lu != %u\n", archive,
                (unsigned long)index, kind, (unsigned long)info.num_chunks,
                expected_chunks);
        ok = 0;
    }
    if (directional && (info.num_chunks < 8 || (info.num_chunks & 7u) != 0)) {
        fprintf(stderr, "%s[%lu] %s has %lu chunks; expected complete 8-way groups\n",
                archive, (unsigned long)index, kind,
                (unsigned long)info.num_chunks);
        ok = 0;
    }
    for (chunk_index = 0; chunk_index < info.num_chunks; chunk_index++) {
        Rich4SprSmpChunk chunk;
        uint16_t *pixels = NULL;
        size_t pixel_count;
        uint32_t chunk_digest = 2166136261u;

        if (rich4_spr_smp_get_chunk(data, size, &info, chunk_index, &chunk) !=
                RICH4_SPR_SMP_OK ||
            chunk.width <= 0 || chunk.height <= 0) {
            fprintf(stderr, "%s[%lu] %s chunk %u has invalid geometry\n",
                    archive, (unsigned long)index, kind, chunk_index);
            ok = 0;
            continue;
        }
        if ((size_t)chunk.width > SIZE_MAX / (size_t)chunk.height) {
            fprintf(stderr, "%s[%lu] %s chunk %u overflows pixels\n",
                    archive, (unsigned long)index, kind, chunk_index);
            ok = 0;
            continue;
        }
        pixel_count = (size_t)chunk.width * (size_t)chunk.height;
        pixels = (uint16_t *)calloc(pixel_count, sizeof(*pixels));
        if (pixels == NULL ||
            rich4_spr_smp_decode_rgb555(data, size, &info, &chunk,
                                        pixels, pixel_count) !=
                RICH4_SPR_SMP_OK) {
            fprintf(stderr, "%s[%lu] %s chunk %u does not decode\n",
                    archive, (unsigned long)index, kind, chunk_index);
            ok = 0;
        } else {
            size_t pixel_index;
            chunk_digest ^= (uint32_t)chunk.width;
            chunk_digest *= 16777619u;
            chunk_digest ^= (uint32_t)chunk.height;
            chunk_digest *= 16777619u;
            pixel_digest ^= (uint32_t)chunk.width;
            pixel_digest *= 16777619u;
            pixel_digest ^= (uint32_t)chunk.height;
            pixel_digest *= 16777619u;
            for (pixel_index = 0; pixel_index < pixel_count; pixel_index++) {
                chunk_digest ^= (uint32_t)(pixels[pixel_index] & 0xffu);
                chunk_digest *= 16777619u;
                chunk_digest ^= (uint32_t)(pixels[pixel_index] >> 8);
                chunk_digest *= 16777619u;
                pixel_digest ^= (uint32_t)(pixels[pixel_index] & 0xffu);
                pixel_digest *= 16777619u;
                pixel_digest ^= (uint32_t)(pixels[pixel_index] >> 8);
                pixel_digest *= 16777619u;
            }
            if (expected_chunk_digests != NULL &&
                expected_chunk_digests[chunk_index] != 0 &&
                chunk_digest != expected_chunk_digests[chunk_index]) {
                fprintf(stderr,
                        "%s[%lu] %s chunk %u pixel digest %08lx != %08lx\n",
                        archive, (unsigned long)index, kind, chunk_index,
                        (unsigned long)chunk_digest,
                        (unsigned long)expected_chunk_digests[chunk_index]);
                ok = 0;
            }
        }
        free(pixels);
    }
    if (ok && expected_pixel_digest != 0 &&
        pixel_digest != expected_pixel_digest) {
        fprintf(stderr, "%s[%lu] %s pixel digest %08lx != %08lx\n",
                archive, (unsigned long)index, kind,
                (unsigned long)pixel_digest,
                (unsigned long)expected_pixel_digest);
        ok = 0;
    }
    free(data);
    return ok;
}

static int validate_raw_rgb555_resource(Rich4Mkf *mkf, size_t index,
                                        uint16_t expected_width,
                                        uint16_t expected_height,
                                        const char *archive, const char *kind,
                                        uint32_t expected_pixel_digest)
{
    unsigned char *data = NULL;
    size_t size = 0;
    size_t pixel_count;
    size_t pixel_index;
    uint32_t pixel_digest = 2166136261u;
    int ok = 1;

    if (!read_resource(mkf, index, &data, &size, archive, kind)) return 0;
    if ((size_t)expected_width > SIZE_MAX / (size_t)expected_height) {
        fprintf(stderr, "%s[%lu] %s pixel count overflows\n", archive,
                (unsigned long)index, kind);
        free(data);
        return 0;
    }
    pixel_count = (size_t)expected_width * (size_t)expected_height;
    if (pixel_count > SIZE_MAX / 2u || size != pixel_count * 2u) {
        fprintf(stderr,
                "%s[%lu] %s size %lu != raw RGB555 size %lu\n",
                archive, (unsigned long)index, kind, (unsigned long)size,
                (unsigned long)(pixel_count * 2u));
        free(data);
        return 0;
    }
    pixel_digest ^= (uint32_t)expected_width;
    pixel_digest *= 16777619u;
    pixel_digest ^= (uint32_t)expected_height;
    pixel_digest *= 16777619u;
    for (pixel_index = 0; pixel_index < pixel_count; pixel_index++) {
        uint16_t pixel = (uint16_t)data[pixel_index * 2u] |
                         (uint16_t)data[1u + pixel_index * 2u] << 8;
        pixel_digest ^= (uint32_t)(pixel & 0xffu);
        pixel_digest *= 16777619u;
        pixel_digest ^= (uint32_t)(pixel >> 8);
        pixel_digest *= 16777619u;
    }
    if (expected_pixel_digest != 0 && pixel_digest != expected_pixel_digest) {
        fprintf(stderr, "%s[%lu] %s pixel digest %08lx != %08lx\n",
                archive, (unsigned long)index, kind,
                (unsigned long)pixel_digest,
                (unsigned long)expected_pixel_digest);
        ok = 0;
    }
    free(data);
    return ok;
}

static int validate_flc_resource(Rich4Mkf *mkf, size_t index,
                                 const char *archive, const char *kind,
                                 uint32_t expected_pixel_digest)
{
    unsigned char *data = NULL;
    size_t size = 0;
    Rich4FlcHeader header;
    Rich4FlcDecoder decoder;
    uint16_t *pixels = NULL;
    size_t pixel_count;
    unsigned int frame_index;
    size_t pixel_index;
    uint32_t pixel_digest = 2166136261u;
    int ok = 1;

    if (!read_resource(mkf, index, &data, &size, archive, kind)) return 0;
    if (rich4_flc_parse_header(data, size, &header) != RICH4_FLC_OK ||
        header.width == 0 || header.height == 0 || header.frame_count == 0 ||
        header.delay_ms == 0) {
        fprintf(stderr, "%s[%lu] %s has invalid FLC header\n", archive,
                (unsigned long)index, kind);
        free(data);
        return 0;
    }
    if ((size_t)header.width > SIZE_MAX / (size_t)header.height) {
        fprintf(stderr, "%s[%lu] %s pixel count overflows\n", archive,
                (unsigned long)index, kind);
        free(data);
        return 0;
    }
    pixel_count = (size_t)header.width * (size_t)header.height;
    pixel_digest ^= (uint32_t)header.width;
    pixel_digest *= 16777619u;
    pixel_digest ^= (uint32_t)header.height;
    pixel_digest *= 16777619u;
    pixel_digest ^= (uint32_t)header.frame_count;
    pixel_digest *= 16777619u;
    pixel_digest ^= (uint32_t)header.delay_ms;
    pixel_digest *= 16777619u;
    pixels = (uint16_t *)calloc(pixel_count, sizeof(*pixels));
    if (pixels == NULL ||
        rich4_flc_decoder_init(&decoder, data, size, pixels, pixel_count) !=
            RICH4_FLC_OK) {
        fprintf(stderr, "%s[%lu] %s decoder initialization failed\n",
                archive, (unsigned long)index, kind);
        free(pixels);
        free(data);
        return 0;
    }
    for (frame_index = 0; frame_index < header.frame_count; frame_index++) {
        if (rich4_flc_decode_next(&decoder, NULL) != RICH4_FLC_OK) {
            fprintf(stderr, "%s[%lu] %s frame %u does not decode\n", archive,
                    (unsigned long)index, kind, frame_index);
            ok = 0;
            break;
        }
        pixel_digest ^= frame_index;
        pixel_digest *= 16777619u;
        for (pixel_index = 0; pixel_index < pixel_count; pixel_index++) {
            pixel_digest ^= (uint32_t)(pixels[pixel_index] & 0xffu);
            pixel_digest *= 16777619u;
            pixel_digest ^= (uint32_t)(pixels[pixel_index] >> 8);
            pixel_digest *= 16777619u;
        }
    }
    if (ok && expected_pixel_digest != 0 &&
        pixel_digest != expected_pixel_digest) {
        fprintf(stderr, "%s[%lu] %s pixel digest %08lx != %08lx\n",
                archive, (unsigned long)index, kind,
                (unsigned long)pixel_digest,
                (unsigned long)expected_pixel_digest);
        ok = 0;
    }
    free(pixels);
    free(data);
    return ok;
}

static int validate_mkf_count(Rich4Mkf *mkf, size_t minimum,
                              const char *archive)
{
    size_t count = rich4_mkf_count(mkf);
    if (count < minimum) {
        fprintf(stderr, "%s has %lu resources; expected at least %lu\n",
                archive, (unsigned long)count, (unsigned long)minimum);
        return 0;
    }
    return 1;
}

static int open_mkf(const char *path, Rich4Mkf **out_mkf, const char *archive)
{
    Rich4MkfError error = rich4_mkf_open(path, out_mkf);
    if (error != RICH4_MKF_OK) {
        fprintf(stderr, "cannot open %s [%s]: %s\n", archive,
                rich4_mkf_error_name(error), rich4_mkf_last_error(*out_mkf));
        rich4_mkf_close(*out_mkf);
        *out_mkf = NULL;
        return 0;
    }
    return 1;
}

int main(int argc, char **argv)
{
    static const unsigned int dynamic_chunks[20] = {
        8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
        8, 8, 8, 8, 8, 8, 8, 8, 6, 1
    };
    /* FNV-1a over each chunk's width, height and decoded RGB555 pixels.
     * These are immutable V3.11 resource-level pixel baselines, not SDL
     * framebuffer hashes; they catch a changed asset, chunk order or decoder
     * result before a full-scene screenshot comparison exists. */
    static const uint32_t dynamic_pixel_digests[20] = {
        UINT32_C(0xdc64d89c), UINT32_C(0x77f418bf),
        UINT32_C(0x312a9b42), UINT32_C(0x7ec61eb6),
        UINT32_C(0x125f87e7), UINT32_C(0x91fffc08),
        UINT32_C(0xf96c3973), UINT32_C(0xf263b885),
        UINT32_C(0x986d5b05), UINT32_C(0x96651ab1),
        UINT32_C(0x9508427a), UINT32_C(0x0f4d4345),
        UINT32_C(0x141ffdc8), UINT32_C(0x16f1b7e0),
        UINT32_C(0xe76e8d2d), UINT32_C(0x787f89cc),
        UINT32_C(0x9ef9749f), UINT32_C(0xf52d935c),
        UINT32_C(0x6e19be33), UINT32_C(0x70b655cb)
    };
    /* Per-frame baselines for the dynamic-object direction groups.  Rows
     * with six or one chunks only use the leading entries; zero is reserved
     * for unused trailing slots. */
    static const uint32_t dynamic_chunk_pixel_digests[20][8] = {
        { UINT32_C(0x2615e662), UINT32_C(0x93e6d9b8), UINT32_C(0xdd5c9881), UINT32_C(0x0ebaf538), UINT32_C(0xb75027cb), UINT32_C(0xf49597ea), UINT32_C(0xf595556b), UINT32_C(0xb7af19cc) },
        { UINT32_C(0x1ddd6206), UINT32_C(0x2736f6e7), UINT32_C(0xbede9b39), UINT32_C(0xcdfbd1f0), UINT32_C(0x007e91e6), UINT32_C(0x96bb33ab), UINT32_C(0x9df4fbbc), UINT32_C(0x6fb9966b) },
        { UINT32_C(0xcbc44d3e), UINT32_C(0xe74081d7), UINT32_C(0x5de96bc2), UINT32_C(0x96402e43), UINT32_C(0xd528d396), UINT32_C(0xd79bf3ed), UINT32_C(0x7bd58f20), UINT32_C(0xcf5e0270) },
        { UINT32_C(0xcc9c36f3), UINT32_C(0x068cde89), UINT32_C(0x4deb3ee8), UINT32_C(0xa49692e1), UINT32_C(0x33043a8e), UINT32_C(0x5fe8c7ff), UINT32_C(0xc12cdcf5), UINT32_C(0xb5af1734) },
        { UINT32_C(0xc2dea0a4), UINT32_C(0xb83c9448), UINT32_C(0xede4b3ed), UINT32_C(0xe5e2eefb), UINT32_C(0xde65bd0b), UINT32_C(0x6c520a24), UINT32_C(0xae1c506d), UINT32_C(0x2ced514a) },
        { UINT32_C(0x5e8367e0), UINT32_C(0xc836f915), UINT32_C(0x0b7a399b), UINT32_C(0x077712f1), UINT32_C(0x9dd26ee4), UINT32_C(0xa2d9173b), UINT32_C(0x035bcba8), UINT32_C(0xe6cceb21) },
        { UINT32_C(0xa1200b47), UINT32_C(0xf66f1b17), UINT32_C(0xe970c56e), UINT32_C(0x454b6d29), UINT32_C(0xd0aeba88), UINT32_C(0xa10eba15), UINT32_C(0xc4e78994), UINT32_C(0x51d0f5a4) },
        { UINT32_C(0xc57e9555), UINT32_C(0xcbd9b98c), UINT32_C(0xbc62d90a), UINT32_C(0x5428ed72), UINT32_C(0x58a541b7), UINT32_C(0x0997bb19), UINT32_C(0x20c85ee8), UINT32_C(0x329b8dab) },
        { UINT32_C(0x95317cb2), UINT32_C(0x1b6b5a66), UINT32_C(0x00468dc9), UINT32_C(0x7fb86dcc), UINT32_C(0xecfd7857), UINT32_C(0x50b4d25e), UINT32_C(0x09704425), UINT32_C(0xae8ed9ed) },
        { UINT32_C(0xb40df158), UINT32_C(0xac7f6191), UINT32_C(0xfadc6ed1), UINT32_C(0x413ec986), UINT32_C(0xed6fd932), UINT32_C(0xcc540eca), UINT32_C(0x51028c05), UINT32_C(0xb3c1c35f) },
        { UINT32_C(0xcbb24afa), UINT32_C(0x3fcf212a), UINT32_C(0xb396b067), UINT32_C(0x0633d41b), UINT32_C(0x1faae19d), UINT32_C(0xbf838592), UINT32_C(0x5573b960), UINT32_C(0x36f8ed18) },
        { UINT32_C(0x0d581801), UINT32_C(0xa5327e07), UINT32_C(0x5e7f76ce), UINT32_C(0xad2c6742), UINT32_C(0xfc1cd53e), UINT32_C(0xc1ef2156), UINT32_C(0xed732a1e), UINT32_C(0xc8d2392c) },
        { UINT32_C(0xb5023dc8), UINT32_C(0x148f7ed0), UINT32_C(0x1191c1c5), UINT32_C(0x18c44bad), UINT32_C(0x99843b8f), UINT32_C(0x9e4ef735), UINT32_C(0x631736b9), UINT32_C(0x3ba08e8e) },
        { UINT32_C(0xdb0e26ea), UINT32_C(0x149aa000), UINT32_C(0x35d134da), UINT32_C(0xdd9e6f1f), UINT32_C(0x7c4d28f6), UINT32_C(0xe88a38a7), UINT32_C(0x2d6a2b92), UINT32_C(0x9dfddc7d) },
        { UINT32_C(0x4ddef994), UINT32_C(0xaf57b046), UINT32_C(0x23721cbb), UINT32_C(0x8e39ca82), UINT32_C(0x53c37d11), UINT32_C(0x985af830), UINT32_C(0x5b749f7a), UINT32_C(0x476c1c04) },
        { UINT32_C(0xf8089586), UINT32_C(0x62e348c2), UINT32_C(0x22e9aa96), UINT32_C(0xc1a4de5f), UINT32_C(0xa3258f12), UINT32_C(0x2a7395b0), UINT32_C(0xf46e5889), UINT32_C(0x895de1c3) },
        { UINT32_C(0x22fbe793), UINT32_C(0xa0ee9e0f), UINT32_C(0xe8671166), UINT32_C(0xa9ae9069), UINT32_C(0x05e35220), UINT32_C(0x1fcd19ae), UINT32_C(0xbab3bd65), UINT32_C(0xae9598e6) },
        { UINT32_C(0xc16ad8b4), UINT32_C(0xad6b2d5a), UINT32_C(0x7060e96c), UINT32_C(0x345e21fd), UINT32_C(0x36ec5c41), UINT32_C(0xd1eaef40), UINT32_C(0xf8e866c2), UINT32_C(0x03729865) },
        { UINT32_C(0x894afd30), UINT32_C(0x9e2b3d32), UINT32_C(0x5bf524f4), UINT32_C(0x0d36714c), UINT32_C(0x15120ff2), UINT32_C(0x2cd4fc6e), UINT32_C(0), UINT32_C(0) },
        { UINT32_C(0x70b655cb), UINT32_C(0), UINT32_C(0), UINT32_C(0), UINT32_C(0), UINT32_C(0), UINT32_C(0), UINT32_C(0) }
    };
    static const uint32_t fixed_flc_pixel_digests[4] = {
        UINT32_C(0x3508b4eb), UINT32_C(0xc84f2e43),
        UINT32_C(0xa13d6a3a), UINT32_C(0xa83c26c2)
    };
    static const uint32_t god_flc_pixel_digests[12] = {
        UINT32_C(0x40cfc328), UINT32_C(0x299d617b),
        UINT32_C(0x334571ec), UINT32_C(0xee27598b),
        UINT32_C(0xf305ad92), UINT32_C(0x5a1251a5),
        UINT32_C(0xefd3b10b), UINT32_C(0xc0ececc0),
        UINT32_C(0x5b94fc42), UINT32_C(0xbd9c1afe),
        UINT32_C(0x79fe3771), UINT32_C(0xb8c225b5)
    };
    /* Types 11, 13 and 14 have no acquisition callback in V3.11. */
    static const size_t god_resources[] = {
        540, 541, 542, 543, 544, 545, 546, 547, 548, 549, 550, 551
    };
    /* These are the seven FLCs selected by the runtime tool-effect catalog:
     * max-level property, explosion, missile, nuclear, alien, worker and
     * travel/disappearance.  Locking their decoded pixels extends the
     * resource-level baseline to the remaining M3.6/M3.7 presentations. */
    static const size_t tool_flc_resources[] = {
        523, 525, 528, 530, 533, 553, 558
    };
    static const uint32_t tool_flc_pixel_digests[] = {
        UINT32_C(0xdddfefc1), UINT32_C(0xa6b7f90f),
        UINT32_C(0x8c4e3a27), UINT32_C(0x691b7e85),
        UINT32_C(0x632d083d), UINT32_C(0x7689b350),
        UINT32_C(0xdfdc5d30)
    };
    /* Remaining FLCs with direct runtime users: special-event phase-1 /
     * point-game/reward presentations and first-turn landing animations. */
    static const size_t runtime_flc_resources[] = {
        526, 527, 531, 534, 535, 536, 537, 539,
        559, 560, 561, 562, 563, 564, 565, 566, 567, 568, 569, 570
    };
    static const uint32_t runtime_flc_pixel_digests[20] = {
        UINT32_C(0xfdd8f160), UINT32_C(0x6896ff3b),
        UINT32_C(0x67ec63bb), UINT32_C(0x14ce7138),
        UINT32_C(0x13e1cc8d), UINT32_C(0x2a25755e),
        UINT32_C(0x1e0950d7), UINT32_C(0x46588959),
        UINT32_C(0xcf43990d), UINT32_C(0x6e066074),
        UINT32_C(0x13b80aad), UINT32_C(0xd9aafa1f),
        UINT32_C(0xec0bce5b), UINT32_C(0x700b3a93),
        UINT32_C(0x83b628dd), UINT32_C(0xe60575ae),
        UINT32_C(0x9b6336ff), UINT32_C(0x26f7f085),
        UINT32_C(0x65c82ca2), UINT32_C(0x1bd71b25)
    };
    /* Data.mkf[441..516] are the 76 special-event images.  They are raw
     * 388x251 RGB555 pixels in V3.11; the renderer uses the no-header raw
     * fallback because the first four pixel bytes are not dimensions. */
    static const uint32_t special_event_raw_pixel_digests[76] = {
        UINT32_C(0x22824e12), UINT32_C(0x208e26fb),
        UINT32_C(0xe01d6522), UINT32_C(0x65e64378),
        UINT32_C(0x461e5798), UINT32_C(0xdce21415),
        UINT32_C(0xe97446ec), UINT32_C(0x728b765b),
        UINT32_C(0x472eae3a), UINT32_C(0xb00dfec2),
        UINT32_C(0x02f1a1c0), UINT32_C(0xe405ab3b),
        UINT32_C(0xa562244e), UINT32_C(0x88d96385),
        UINT32_C(0x53d8ed6d), UINT32_C(0xc7ca8b6c),
        UINT32_C(0x79f97cf7), UINT32_C(0xb8850c39),
        UINT32_C(0xc63cf1ec), UINT32_C(0x14cf04a0),
        UINT32_C(0x1a91c290), UINT32_C(0xd96c07d5),
        UINT32_C(0xc87517d3), UINT32_C(0x4fc84dfc),
        UINT32_C(0xddd46049), UINT32_C(0x05b3ba6f),
        UINT32_C(0xdec499c4), UINT32_C(0xdec499c4),
        UINT32_C(0xf47ad12f), UINT32_C(0x41e0c13d),
        UINT32_C(0xbf333acd), UINT32_C(0xe9ad5e4c),
        UINT32_C(0xaabdfb55), UINT32_C(0xcf4a5fef),
        UINT32_C(0x8fcfd59e), UINT32_C(0x0f24e7b4),
        UINT32_C(0x64c4ac94), UINT32_C(0xc9c5cde5),
        UINT32_C(0x7b1fbf64), UINT32_C(0x9f9cd111),
        UINT32_C(0xb5a4e3e6), UINT32_C(0x8bc4cbb0),
        UINT32_C(0xb2e920fe), UINT32_C(0x689cbedc),
        UINT32_C(0xdbba084c), UINT32_C(0x1a589e4a),
        UINT32_C(0x704ff5b1), UINT32_C(0xf998ef1b),
        UINT32_C(0x5c3932cd), UINT32_C(0x9f9226e1),
        UINT32_C(0x756dee4d), UINT32_C(0x1e2c3ff2),
        UINT32_C(0x590f299c), UINT32_C(0xcaf79312),
        UINT32_C(0xf7832eba), UINT32_C(0x483db250),
        UINT32_C(0x34e167fe), UINT32_C(0x2f9b1794),
        UINT32_C(0x4cf5161d), UINT32_C(0xfafd6fd4),
        UINT32_C(0x09bf1a35), UINT32_C(0xfde9ceae),
        UINT32_C(0x43714028), UINT32_C(0x4879b1e0),
        UINT32_C(0x3171b06c), UINT32_C(0xd9a2cad9),
        UINT32_C(0xbae0c5e0), UINT32_C(0x814fec6b),
        UINT32_C(0xed6e6ab0), UINT32_C(0x5d691102),
        UINT32_C(0xf8e3731a), UINT32_C(0xea6338f7),
        UINT32_C(0xef75511d), UINT32_C(0xe91fa030),
        UINT32_C(0x5a4bfaf3), UINT32_C(0xf27fffd4)
    };
    Rich4Mkf *data_mkf = NULL;
    Rich4Mkf *effect_mkf = NULL;
    Rich4Mkf *panel_mkf = NULL;
    size_t i;
    int ok = 1;

    if (argc != 4) {
        fprintf(stderr,
                "usage: mkf_validate_m37_resources <Data.mkf> "
                "<Effect.mkf> <Panel.mkf>\n");
        return 2;
    }
    if (!open_mkf(argv[1], &data_mkf, "Data.mkf") ||
        !open_mkf(argv[2], &effect_mkf, "Effect.mkf") ||
        !open_mkf(argv[3], &panel_mkf, "Panel.mkf")) {
        rich4_mkf_close(data_mkf);
        rich4_mkf_close(effect_mkf);
        rich4_mkf_close(panel_mkf);
        return 1;
    }

    ok = validate_mkf_count(data_mkf, 552, "Data.mkf") && ok;
    ok = validate_mkf_count(effect_mkf, 0x72, "Effect.mkf") && ok;
    ok = validate_mkf_count(panel_mkf, 66, "Panel.mkf") && ok;

    for (i = 0; i < 20; i++)
        ok = validate_spr_resource(data_mkf, 396 + i, dynamic_chunks[i], 0,
                                   "Data.mkf", "dynamic-object SPR",
                                   dynamic_pixel_digests[i],
                                   dynamic_chunk_pixel_digests[i]) && ok;
    for (i = 0; i < 12 * 21; i++)
        ok = validate_spr_resource(data_mkf, 128 + i, 0, 1,
                                   "Data.mkf", "ordinary actor SPR", 0, NULL) && ok;
    for (i = 0; i < 4 * 4; i++)
        ok = validate_spr_resource(data_mkf, 380 + i, 0, 1,
                                   "Data.mkf", "special actor SPR", 0, NULL) && ok;
    ok = validate_flc_resource(data_mkf, 524, "Data.mkf", "hospital FLC",
                               fixed_flc_pixel_digests[0]) &&
        ok;
    ok = validate_flc_resource(data_mkf, 538, "Data.mkf", "prison FLC",
                               fixed_flc_pixel_digests[1]) &&
        ok;
    ok = validate_flc_resource(data_mkf, 529, "Data.mkf",
                               "demolish-card FLC", fixed_flc_pixel_digests[2]) && ok;
    ok = validate_flc_resource(data_mkf, 557, "Data.mkf",
                               "monster-card FLC", fixed_flc_pixel_digests[3]) && ok;
    for (i = 0; i < sizeof(tool_flc_resources) / sizeof(tool_flc_resources[0]); i++)
        ok = validate_flc_resource(data_mkf, tool_flc_resources[i],
                                   "Data.mkf", "tool-effect FLC",
                                   tool_flc_pixel_digests[i]) && ok;
    for (i = 0; i < 76; i++)
        ok = validate_raw_rgb555_resource(
                 data_mkf, 441 + i, 388, 251, "Data.mkf",
                 "special-event raw image", special_event_raw_pixel_digests[i]) &&
             ok;
    for (i = 0; i < sizeof(runtime_flc_resources) /
                       sizeof(runtime_flc_resources[0]); i++)
        ok = validate_flc_resource(data_mkf, runtime_flc_resources[i],
                                   "Data.mkf", "runtime presentation FLC",
                                   runtime_flc_pixel_digests[i]) && ok;
    for (i = 0; i < sizeof(god_resources) / sizeof(god_resources[0]); i++)
        ok = validate_flc_resource(data_mkf, god_resources[i],
                                   "Data.mkf", "god-acquisition FLC",
                                   god_flc_pixel_digests[i]) && ok;

    for (i = 0x66; i <= 0x71; i++) {
        Rich4MkfResourceInfo info;
        if (rich4_mkf_get_info(effect_mkf, i, &info) != RICH4_MKF_OK ||
            info.uncompressed_size == 0 || info.compressed_size == 0) {
            fprintf(stderr, "Effect.mkf[%lu] god-effect audio is empty\n",
                    (unsigned long)i);
            ok = 0;
        }
    }
    for (i = 63; i <= 65; i++)
        ok = validate_spr_resource(panel_mkf, i, 0, 0, "Panel.mkf",
                                   "confinement panel", 0, NULL) && ok;

    rich4_mkf_close(data_mkf);
    rich4_mkf_close(effect_mkf);
    rich4_mkf_close(panel_mkf);
    if (ok)
        puts("M3.6/M3.7/M3.8 original resources: ok");
    return ok ? 0 : 1;
}
