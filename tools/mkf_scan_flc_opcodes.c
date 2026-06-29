#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "resource/mkf.h"
#include "resource/flc_animation.h"
#include "resource/flc_player.h"

static uint16_t read_u16le(const unsigned char *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_u32le(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int main(int argc, char **argv)
{
    Rich4Mkf *mkf = NULL;
    Rich4MkfError mkf_error;
    size_t resource;
    size_t flc_count = 0;
    size_t frame_count = 0;
    size_t failed_resources = 0;

    if (argc != 2) {
        fprintf(stderr, "usage: mkf_scan_flc_opcodes <path-to-mkf>\n");
        return 2;
    }
    mkf_error = rich4_mkf_open(argv[1], &mkf);
    if (mkf_error != RICH4_MKF_OK) {
        fprintf(stderr, "error: cannot open MKF [%s]\n", rich4_mkf_error_name(mkf_error));
        rich4_mkf_close(mkf);
        return 1;
    }
    for (resource = 0; resource < rich4_mkf_count(mkf); resource++) {
        unsigned char *data = NULL;
        size_t size = 0;
        Rich4FlcHeader header;
        Rich4FlcPlayer player;
        uint16_t *pixels;
        unsigned char *indices;
        unsigned char *dirty;
        size_t pixel_count;
        unsigned int frame_index;
        if (rich4_mkf_read_resource(mkf, resource, &data, &size, NULL) != RICH4_MKF_OK ||
            rich4_flc_parse_header(data, size, &header) != RICH4_FLC_OK) {
            free(data);
            continue;
        }
        pixel_count = (size_t)header.width * header.height;
        pixels = (uint16_t *)calloc(pixel_count, sizeof(*pixels));
        indices = (unsigned char *)calloc(pixel_count, 1);
        dirty = (unsigned char *)calloc(pixel_count, 1);
        if (pixels == NULL || indices == NULL || dirty == NULL) {
            free(dirty);
            free(indices);
            free(data);
            free(pixels);
            rich4_mkf_close(mkf);
            return 1;
        }
        if (rich4_flc_player_init(&player, data, size, pixels, indices, dirty,
                                  pixel_count, 0) != RICH4_FLC_OK) {
            free(dirty);
            free(indices);
            free(pixels);
            free(data);
            continue;
        }
        flc_count++;
        for (frame_index = 0; frame_index < header.frame_count; frame_index++) {
            Rich4FlcFrame frame;
            Rich4FlcError error;
            size_t chunk_offset;
            unsigned int chunk;
            int advanced = 0;
            error = rich4_flc_player_advance(&player,
                                             frame_index * player.delay_ms,
                                             &advanced);
            if (error != RICH4_FLC_OK) {
                printf("resource=%lu frame=%u error=%s\n",
                       (unsigned long)resource, frame_index, rich4_flc_error_name(error));
                failed_resources++;
                break;
            }
            if (!advanced || !player.has_frame ||
                player.current_frame != (int)frame_index) {
                printf("resource=%lu frame=%u error=player-state\n",
                       (unsigned long)resource, frame_index);
                failed_resources++;
                break;
            }
            frame = player.frame;
            chunk_offset = frame.offset + RICH4_FLC_FRAME_HEADER_SIZE;
            for (chunk = 0; chunk < frame.command_count; chunk++) {
                uint32_t chunk_size = read_u32le(data + chunk_offset);
                uint16_t opcode = read_u16le(data + chunk_offset + 4);
                printf("resource=%lu frame=%u opcode=%u chunk_size=%lu\n",
                       (unsigned long)resource,
                       frame_index,
                       (unsigned int)opcode,
                       (unsigned long)chunk_size);
                chunk_offset += chunk_size;
            }
            frame_count++;
        }
        free(dirty);
        free(indices);
        free(pixels);
        free(data);
    }
    printf("summary flc_resources=%lu decoded_frames=%lu failed_resources=%lu\n",
           (unsigned long)flc_count,
           (unsigned long)frame_count,
           (unsigned long)failed_resources);
    rich4_mkf_close(mkf);
    return failed_resources == 0 ? 0 : 1;
}
