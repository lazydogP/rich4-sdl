#include <stdint.h>
#include <stdio.h>

static void write_u16le(FILE *fp, uint16_t value)
{
    fputc((int)(value & 0xff), fp);
    fputc((int)((value >> 8) & 0xff), fp);
}

static void write_u32le(FILE *fp, uint32_t value)
{
    fputc((int)(value & 0xff), fp);
    fputc((int)((value >> 8) & 0xff), fp);
    fputc((int)((value >> 16) & 0xff), fp);
    fputc((int)((value >> 24) & 0xff), fp);
}

static void write_resource_header(FILE *fp,
                                  uint32_t uncompressed_size,
                                  uint32_t compressed_size,
                                  uint32_t image_data_offset,
                                  uint32_t image_data_size)
{
    write_u32le(fp, uncompressed_size);
    write_u32le(fp, compressed_size);
    write_u32le(fp, image_data_offset);
    write_u32le(fp, image_data_size);
}

int main(int argc, char **argv)
{
    FILE *fp;

    if (argc != 2) {
        fprintf(stderr, "usage: mkf_make_sample <output-mkf>\n");
        return 2;
    }

    fp = fopen(argv[1], "wb");
    if (fp == NULL) {
        perror("cannot create sample MKF");
        return 1;
    }

    write_u32le(fp, 64); /* index table offset */

    write_resource_header(fp, 28, 28, 0, 28);
    fwrite("SMP\0", 1, 4, fp);
    write_u32le(fp, 1);  /* chunk count */
    write_u32le(fp, 24); /* chunk data starts after signature and table */
    write_u16le(fp, 2);  /* width */
    write_u16le(fp, 2);  /* height */
    write_u16le(fp, 1);  /* x */
    write_u16le(fp, (uint16_t)-1); /* y */
    write_u32le(fp, 4);  /* chunk data size */
    fputc(1, fp);
    fputc(2, fp);
    fputc(3, fp);
    fputc(4, fp);

    write_resource_header(fp, 0, 0, 0, 0); /* zero-length resource */

    write_u32le(fp, 4);
    write_u32le(fp, 48);

    if (fclose(fp) != 0) {
        perror("cannot finish sample MKF");
        return 1;
    }
    return 0;
}
