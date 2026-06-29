#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#define MKDIR(path) mkdir(path, 0777)
#endif

#include "resource/mkf.h"
#include "resource/flc_animation.h"

static uint16_t read_u16le(const unsigned char *p);
static uint32_t read_u32le(const unsigned char *p);

static uint16_t read_u16le(const unsigned char *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t read_u32le(const unsigned char *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static void print_frame_opcodes(const unsigned char *data,
                                size_t size,
                                const Rich4FlcFrame *frame)
{
    size_t offset;
    size_t end;
    int command;

    if (data == NULL || frame == NULL || frame->offset + frame->size > size) {
        return;
    }
    offset = frame->offset + 16;
    end = frame->offset + frame->size;
    printf(" opcodes=");
    for (command = 0; command < (int)frame->command_count && offset + 6 <= end; command++) {
        uint32_t command_size = read_u32le(data + offset);
        uint16_t opcode = read_u16le(data + offset + 4);
        if (command_size < 6 || offset + command_size > end) {
            printf(" bad");
            break;
        }
        printf("%s%u", command == 0 ? "" : ",", (unsigned int)opcode);
        offset += command_size;
    }
}

static void write_u16(FILE *fp, uint16_t v)
{
    fputc((int)(v & 0xff), fp);
    fputc((int)((v >> 8) & 0xff), fp);
}

static void write_u32(FILE *fp, uint32_t v)
{
    fputc((int)(v & 0xff), fp);
    fputc((int)((v >> 8) & 0xff), fp);
    fputc((int)((v >> 16) & 0xff), fp);
    fputc((int)((v >> 24) & 0xff), fp);
}

static int write_bmp(const char *path, const uint16_t *fb, int width, int height)
{
    int row_stride = ((width * 3 + 3) / 4) * 4;
    uint32_t image_size = (uint32_t)(row_stride * height);
    uint32_t file_size = 54u + image_size;
    unsigned char *row = NULL;
    FILE *fp;
    int y;

    fp = fopen(path, "wb");
    if (fp == NULL) {
        fprintf(stderr, "error: cannot write %s: %s\n", path, strerror(errno));
        return 0;
    }
    row = (unsigned char *)calloc((size_t)row_stride, 1);
    if (row == NULL) {
        fclose(fp);
        return 0;
    }

    fputc('B', fp);
    fputc('M', fp);
    write_u32(fp, file_size);
    write_u16(fp, 0);
    write_u16(fp, 0);
    write_u32(fp, 54);
    write_u32(fp, 40);
    write_u32(fp, (uint32_t)width);
    write_u32(fp, (uint32_t)height);
    write_u16(fp, 1);
    write_u16(fp, 24);
    write_u32(fp, 0);
    write_u32(fp, image_size);
    write_u32(fp, 2835);
    write_u32(fp, 2835);
    write_u32(fp, 0);
    write_u32(fp, 0);

    for (y = height - 1; y >= 0; y--) {
        int x;
        memset(row, 0, (size_t)row_stride);
        for (x = 0; x < width; x++) {
            uint16_t c = fb[y * width + x];
            unsigned char r = (unsigned char)(((c >> 11) & 0x1f) * 255 / 31);
            unsigned char g = (unsigned char)(((c >> 5) & 0x3f) * 255 / 63);
            unsigned char b = (unsigned char)((c & 0x1f) * 255 / 31);
            row[x * 3 + 0] = b;
            row[x * 3 + 1] = g;
            row[x * 3 + 2] = r;
        }
        fwrite(row, 1, (size_t)row_stride, fp);
    }
    free(row);
    if (fclose(fp) != 0) {
        return 0;
    }
    return 1;
}


static void fill_checker(uint16_t *fb, int width, int height)
{
    int y;
    if (fb == NULL) {
        return;
    }
    for (y = 0; y < height; y++) {
        int x;
        for (x = 0; x < width; x++) {
            int shade = ((x / 8) + (y / 8)) & 1;
            fb[y * width + x] = shade ? 0x7bef : 0x39e7;
        }
    }
}


static void print_usage(FILE *fp)
{
    fprintf(fp, "usage: mkf_dump_flc_frames <path-to-mkf> <resource-index> <out-dir>\n");
}

int main(int argc, char **argv)
{
    Rich4Mkf *mkf = NULL;
    unsigned char *data = NULL;
    size_t data_size = 0;
    Rich4FlcHeader header;
    Rich4FlcDecoder cumulative;
    int resource;
    int parsed_frames = 0;
    int i;
    size_t pixel_count;
    uint16_t *cum_fb = NULL;
    uint16_t *single_fb = NULL;
    uint16_t *source_fb = NULL;
    uint16_t *resolved_fb = NULL;
    uint16_t *resolved_prev = NULL;
    char path[512];
    Rich4MkfError mkf_error;
    Rich4FlcError flc_error;

    if (argc != 4) {
        print_usage(stderr);
        return 2;
    }
    resource = atoi(argv[2]);
    if (resource < 0) {
        fprintf(stderr, "error: resource index must be non-negative\n");
        return 2;
    }
    if (MKDIR(argv[3]) != 0 && errno != EEXIST) {
        fprintf(stderr, "error: cannot create output directory %s: %s\n",
                argv[3], strerror(errno));
        return 1;
    }
    mkf_error = rich4_mkf_open(argv[1], &mkf);
    if (mkf_error != RICH4_MKF_OK) {
        fprintf(stderr, "error: open failed [%s]: %s\n",
                rich4_mkf_error_name(mkf_error), rich4_mkf_last_error(mkf));
        rich4_mkf_close(mkf);
        return 1;
    }
    mkf_error = rich4_mkf_read_resource(mkf, (size_t)resource,
                                        &data, &data_size, NULL);
    if (mkf_error != RICH4_MKF_OK) {
        fprintf(stderr, "error: read resource failed [%s]: %s\n",
                rich4_mkf_error_name(mkf_error), rich4_mkf_last_error(mkf));
        rich4_mkf_close(mkf);
        return 1;
    }
    rich4_mkf_close(mkf);

    flc_error = rich4_flc_parse_header(data, data_size, &header);
    if (flc_error != RICH4_FLC_OK) {
        fprintf(stderr, "error: resource %d is not a valid Autodesk Animator FLC [%s]\n",
                resource, rich4_flc_error_name(flc_error));
        free(data);
        return 1;
    }
    pixel_count = (size_t)header.width * header.height;
    cum_fb = (uint16_t *)calloc(pixel_count, sizeof(*cum_fb));
    single_fb = (uint16_t *)calloc(pixel_count, sizeof(*single_fb));
    source_fb = (uint16_t *)calloc(pixel_count, sizeof(*source_fb));
    resolved_fb = (uint16_t *)calloc(pixel_count, sizeof(*resolved_fb));
    resolved_prev = (uint16_t *)calloc(pixel_count, sizeof(*resolved_prev));
    if (cum_fb == NULL || single_fb == NULL || source_fb == NULL ||
        resolved_fb == NULL || resolved_prev == NULL) {
        fprintf(stderr, "error: out of memory\n");
        free(cum_fb);
        free(single_fb);
        free(source_fb);
        free(resolved_fb);
        free(resolved_prev);
        free(data);
        return 1;
    }
    flc_error = rich4_flc_decoder_init(&cumulative, data, data_size,
                                       cum_fb, pixel_count);
    if (flc_error != RICH4_FLC_OK) {
        fprintf(stderr, "error: decoder init failed [%s]\n",
                rich4_flc_error_name(flc_error));
        free(cum_fb);
        free(single_fb);
        free(source_fb);
        free(resolved_fb);
        free(resolved_prev);
        free(data);
        return 1;
    }

    printf("resource=%d width=%u height=%u header_frames=%u delay_ms=%lu\n",
           resource, (unsigned int)header.width, (unsigned int)header.height,
           (unsigned int)header.frame_count, (unsigned long)header.delay_ms);

    for (i = 0; i < (int)header.frame_count; i++) {
        Rich4FlcFrame frame;
        Rich4FlcDecoder single;
        Rich4FlcDecoder checker;
        Rich4FlcDecoder resolved;
        int j;

        flc_error = rich4_flc_decode_next(&cumulative, &frame);
        if (flc_error != RICH4_FLC_OK) {
            fprintf(stderr, "error: decode cumulative frame %d failed [%s]\n",
                    i, rich4_flc_error_name(flc_error));
            break;
        }
        snprintf(path, sizeof(path), "%s/resource_%02d_cum_%03d.bmp",
                 argv[3], resource, i);
        write_bmp(path, cum_fb, header.width, header.height);

        rich4_flc_decoder_init(&single, data, data_size, single_fb, pixel_count);
        memset(source_fb, 0, pixel_count * sizeof(*source_fb));
        rich4_flc_decoder_set_source(&single, source_fb, pixel_count);
        for (j = 0; j <= i; j++) {
            flc_error = rich4_flc_decode_next_ex(&single, NULL, j == i);
            if (flc_error != RICH4_FLC_OK) break;
        }
        if (flc_error != RICH4_FLC_OK) {
            fprintf(stderr, "error: decode single frame %d failed [%s]\n",
                    i, rich4_flc_error_name(flc_error));
            break;
        }
        snprintf(path, sizeof(path), "%s/resource_%02d_single_%03d.bmp",
                 argv[3], resource, i);
        write_bmp(path, single_fb, header.width, header.height);

        rich4_flc_decoder_init(&checker, data, data_size, single_fb, pixel_count);
        fill_checker(single_fb, header.width, header.height);
        fill_checker(source_fb, header.width, header.height);
        rich4_flc_decoder_set_source(&checker, source_fb, pixel_count);
        for (j = 0; j <= i; j++) {
            flc_error = rich4_flc_decode_next_ex(&checker, NULL, j == i);
            if (flc_error != RICH4_FLC_OK) break;
        }
        if (flc_error != RICH4_FLC_OK) {
            fprintf(stderr, "error: decode checker frame %d failed [%s]\n",
                    i, rich4_flc_error_name(flc_error));
            break;
        }
        snprintf(path, sizeof(path), "%s/resource_%02d_single_bg_%03d.bmp",
                 argv[3], resource, i);
        write_bmp(path, single_fb, header.width, header.height);

        rich4_flc_decoder_init(&resolved, data, data_size, resolved_fb, pixel_count);
        rich4_flc_decoder_set_source(&resolved, resolved_prev, pixel_count);
        for (j = 0; j <= i; j++) {
            flc_error = rich4_flc_decode_next_ex(&resolved, NULL, j == i);
            if (flc_error != RICH4_FLC_OK) break;
        }
        if (flc_error != RICH4_FLC_OK) {
            fprintf(stderr, "error: decode resolved frame %d failed [%s]\n",
                    i, rich4_flc_error_name(flc_error));
            break;
        }
        snprintf(path, sizeof(path), "%s/resource_%02d_single_resolved_%03d.bmp",
                 argv[3], resource, i);
        write_bmp(path, resolved_fb, header.width, header.height);
        memcpy(resolved_prev, resolved_fb, pixel_count * sizeof(*resolved_prev));

        printf("frame=%03d offset=%lu size=%lu commands=%u\n",
               i, (unsigned long)frame.offset, (unsigned long)frame.size,
               (unsigned int)frame.command_count);
        print_frame_opcodes(data, data_size, &frame);
        printf("\n");
        parsed_frames++;
    }

    printf("parsed_frames=%d\n", parsed_frames);
    free(cum_fb);
    free(single_fb);
    free(source_fb);
    free(resolved_fb);
    free(resolved_prev);
    free(data);
    return parsed_frames == (int)header.frame_count ? 0 : 1;
}
