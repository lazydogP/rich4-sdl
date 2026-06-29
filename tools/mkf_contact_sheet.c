#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "resource/mkf.h"
#include "resource/spr_smp.h"

typedef struct Image {
    int w;
    int h;
    unsigned char *rgb;
} Image;

static uint16_t read_le16(const unsigned char *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t rgb555_to_rgb24(uint16_t color)
{
    uint32_t r = (uint32_t)(((color >> 10) & 0x1f) * 255 / 31);
    uint32_t g = (uint32_t)(((color >> 5) & 0x1f) * 255 / 31);
    uint32_t b = (uint32_t)((color & 0x1f) * 255 / 31);
    return (r << 16) | (g << 8) | b;
}

static void put_le16(unsigned char *p, uint16_t v)
{
    p[0] = (unsigned char)(v & 0xff);
    p[1] = (unsigned char)((v >> 8) & 0xff);
}

static void put_le32(unsigned char *p, uint32_t v)
{
    p[0] = (unsigned char)(v & 0xff);
    p[1] = (unsigned char)((v >> 8) & 0xff);
    p[2] = (unsigned char)((v >> 16) & 0xff);
    p[3] = (unsigned char)((v >> 24) & 0xff);
}

static int image_init(Image *img, int w, int h, unsigned char bg)
{
    size_t size;

    if (img == NULL || w <= 0 || h <= 0) {
        return 0;
    }
    size = (size_t)w * (size_t)h * 3u;
    img->w = w;
    img->h = h;
    img->rgb = (unsigned char *)malloc(size);
    if (img->rgb == NULL) {
        return 0;
    }
    memset(img->rgb, bg, size);
    return 1;
}

static void image_free(Image *img)
{
    if (img != NULL) {
        free(img->rgb);
        img->rgb = NULL;
    }
}

static void set_pixel(Image *img, int x, int y, uint32_t rgb)
{
    unsigned char *p;

    if (img == NULL || img->rgb == NULL || x < 0 || y < 0 || x >= img->w || y >= img->h) {
        return;
    }
    p = img->rgb + ((size_t)y * (size_t)img->w + (size_t)x) * 3u;
    p[0] = (unsigned char)((rgb >> 16) & 0xff);
    p[1] = (unsigned char)((rgb >> 8) & 0xff);
    p[2] = (unsigned char)(rgb & 0xff);
}

static void draw_rect(Image *img, int x, int y, int w, int h, uint32_t rgb)
{
    int px;
    int py;

    for (py = y; py < y + h; py++) {
        for (px = x; px < x + w; px++) {
            set_pixel(img, px, py, rgb);
        }
    }
}

static void draw_chunk(Image *img,
                       const unsigned char *data,
                       size_t size,
                       const Rich4SprSmpInfo *spr,
                       const Rich4SprSmpChunk *chunk,
                       int center_x,
                       int center_y)
{
    int x;
    int y;
    int dst_x;
    int dst_y;

    if (img == NULL || data == NULL || spr == NULL || chunk == NULL ||
        chunk->width <= 0 || chunk->height <= 0 ||
        chunk->data_offset > size || chunk->data_size > size - chunk->data_offset) {
        return;
    }
    dst_x = center_x - chunk->x;
    dst_y = center_y - chunk->y;
    if (memcmp(spr->signature, "SPR", 4) == 0) {
        const unsigned char *src;
        const unsigned char *pal;

        if (spr->start_offset > size || size - spr->start_offset < 512 ||
            chunk->data_size < (uint32_t)(chunk->width * chunk->height)) {
            return;
        }
        src = data + chunk->data_offset;
        pal = data + spr->start_offset;
        for (y = 0; y < chunk->height; y++) {
            for (x = 0; x < chunk->width; x++) {
                unsigned char index = src[y * chunk->width + x];
                if (index != 0) {
                    uint16_t color = read_le16(pal + (size_t)index * 2u);
                    set_pixel(img, dst_x + x, dst_y + y, rgb555_to_rgb24(color));
                }
            }
        }
    } else {
        const unsigned char *src;

        if (chunk->data_size < (uint32_t)(chunk->width * chunk->height * 2)) {
            return;
        }
        src = data + chunk->data_offset;
        for (y = 0; y < chunk->height; y++) {
            for (x = 0; x < chunk->width; x++) {
                uint16_t color = read_le16(src + ((size_t)y * (size_t)chunk->width + (size_t)x) * 2u);
                if (color != 0) {
                    set_pixel(img, dst_x + x, dst_y + y, rgb555_to_rgb24(color));
                }
            }
        }
    }
}

static int write_bmp(const char *path, const Image *img)
{
    FILE *fp;
    int row_stride;
    uint32_t pixel_size;
    uint32_t file_size;
    unsigned char header[54];
    unsigned char pad[3] = { 0, 0, 0 };
    int y;

    if (path == NULL || img == NULL || img->rgb == NULL) {
        return 0;
    }
    row_stride = (img->w * 3 + 3) & ~3;
    pixel_size = (uint32_t)(row_stride * img->h);
    file_size = 54u + pixel_size;
    memset(header, 0, sizeof(header));
    header[0] = 'B';
    header[1] = 'M';
    put_le32(header + 2, file_size);
    put_le32(header + 10, 54);
    put_le32(header + 14, 40);
    put_le32(header + 18, (uint32_t)img->w);
    put_le32(header + 22, (uint32_t)img->h);
    put_le16(header + 26, 1);
    put_le16(header + 28, 24);
    put_le32(header + 34, pixel_size);

    fp = fopen(path, "wb");
    if (fp == NULL) {
        return 0;
    }
    if (fwrite(header, 1, sizeof(header), fp) != sizeof(header)) {
        fclose(fp);
        return 0;
    }
    for (y = img->h - 1; y >= 0; y--) {
        int x;
        const unsigned char *row = img->rgb + (size_t)y * (size_t)img->w * 3u;
        for (x = 0; x < img->w; x++) {
            unsigned char bgr[3];
            bgr[0] = row[x * 3 + 2];
            bgr[1] = row[x * 3 + 1];
            bgr[2] = row[x * 3 + 0];
            if (fwrite(bgr, 1, sizeof(bgr), fp) != sizeof(bgr)) {
                fclose(fp);
                return 0;
            }
        }
        if (row_stride > img->w * 3 &&
            fwrite(pad, 1, (size_t)(row_stride - img->w * 3), fp) != (size_t)(row_stride - img->w * 3)) {
            fclose(fp);
            return 0;
        }
    }
    fclose(fp);
    return 1;
}

static void usage(FILE *out)
{
    fprintf(out, "usage: mkf_contact_sheet <mkf> <resource> <out.bmp>\n");
}

int main(int argc, char **argv)
{
    Rich4Mkf *mkf = NULL;
    Rich4MkfError err;
    Rich4SprSmpError spr_err;
    unsigned char *data = NULL;
    size_t size = 0;
    Rich4SprSmpInfo spr;
    Rich4SprSmpChunk chunk;
    Image img;
    unsigned long resource;
    char *end = NULL;
    int max_w = 0;
    int max_h = 0;
    int cols;
    int rows;
    int cell_w;
    int cell_h;
    size_t i;

    memset(&img, 0, sizeof(img));
    if (argc != 4) {
        usage(stderr);
        return 2;
    }
    resource = strtoul(argv[2], &end, 10);
    if (end == NULL || *end != '\0') {
        usage(stderr);
        return 2;
    }
    err = rich4_mkf_open(argv[1], &mkf);
    if (err != RICH4_MKF_OK) {
        fprintf(stderr, "open failed [%s]: %s\n", rich4_mkf_error_name(err), rich4_mkf_last_error(mkf));
        rich4_mkf_close(mkf);
        return 1;
    }
    err = rich4_mkf_read_resource(mkf, (size_t)resource, &data, &size, NULL);
    if (err != RICH4_MKF_OK) {
        fprintf(stderr, "read resource failed [%s]: %s\n", rich4_mkf_error_name(err), rich4_mkf_last_error(mkf));
        free(data);
        rich4_mkf_close(mkf);
        return 1;
    }
    if (!rich4_spr_smp_probe(data, size, &spr) || spr.num_chunks == 0) {
        fprintf(stderr, "resource %lu is not SPR/SMP\n", resource);
        free(data);
        rich4_mkf_close(mkf);
        return 1;
    }
    for (i = 0; i < spr.num_chunks; i++) {
        spr_err = rich4_spr_smp_get_chunk(data, size, &spr, i, &chunk);
        if (spr_err != RICH4_SPR_SMP_OK) {
            fprintf(stderr, "chunk %lu parse failed [%s]\n",
                    (unsigned long)i, rich4_spr_smp_error_name(spr_err));
            free(data);
            rich4_mkf_close(mkf);
            return 1;
        }
        if (chunk.width > max_w) max_w = chunk.width;
        if (chunk.height > max_h) max_h = chunk.height;
    }
    cols = 8;
    if (spr.num_chunks < 8) {
        cols = (int)spr.num_chunks;
    }
    rows = ((int)spr.num_chunks + cols - 1) / cols;
    cell_w = max_w + 24;
    cell_h = max_h + 24;
    if (!image_init(&img, cols * cell_w + 1, rows * cell_h + 1, 238)) {
        fprintf(stderr, "out of memory\n");
        free(data);
        rich4_mkf_close(mkf);
        return 1;
    }
    for (i = 0; i < spr.num_chunks; i++) {
        int col = (int)i % cols;
        int row = (int)i / cols;
        int x0 = col * cell_w;
        int y0 = row * cell_h;

        draw_rect(&img, x0, y0, cell_w, 1, 0x808080);
        draw_rect(&img, x0, y0, 1, cell_h, 0x808080);
        spr_err = rich4_spr_smp_get_chunk(data, size, &spr, i, &chunk);
        if (spr_err == RICH4_SPR_SMP_OK) {
            draw_chunk(&img, data, size, &spr, &chunk, x0 + cell_w / 2, y0 + cell_h / 2);
        }
    }
    draw_rect(&img, 0, rows * cell_h, cols * cell_w + 1, 1, 0x808080);
    draw_rect(&img, cols * cell_w, 0, 1, rows * cell_h + 1, 0x808080);
    if (!write_bmp(argv[3], &img)) {
        fprintf(stderr, "cannot write %s\n", argv[3]);
        image_free(&img);
        free(data);
        rich4_mkf_close(mkf);
        return 1;
    }
    printf("wrote %s: resource %lu %.3s chunks=%lu cell=%dx%d\n",
           argv[3], resource, spr.signature, (unsigned long)spr.num_chunks, cell_w, cell_h);
    image_free(&img);
    free(data);
    rich4_mkf_close(mkf);
    return 0;
}
