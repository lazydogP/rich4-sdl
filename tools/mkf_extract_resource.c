#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "resource/mkf.h"

static int parse_index(const char *text, size_t *out_index)
{
    char *end = NULL;
    unsigned long value;

    if (text == NULL || *text == '\0' || out_index == NULL) {
        return 0;
    }
    value = strtoul(text, &end, 10);
    if (*end != '\0') {
        return 0;
    }
    *out_index = (size_t)value;
    return 1;
}

static void print_usage(FILE *out)
{
    fprintf(out, "usage: mkf_extract_resource <path-to-mkf> <resource-index> <output-file>\n");
}

int main(int argc, char **argv)
{
    Rich4Mkf *mkf = NULL;
    Rich4MkfError error;
    unsigned char *data = NULL;
    size_t data_size = 0;
    size_t index;
    FILE *output;

    if (argc != 4 || !parse_index(argv[2], &index)) {
        print_usage(stderr);
        return 2;
    }

    error = rich4_mkf_open(argv[1], &mkf);
    if (error != RICH4_MKF_OK) {
        fprintf(stderr, "error: open failed [%s]: %s\n",
                rich4_mkf_error_name(error),
                rich4_mkf_last_error(mkf));
        rich4_mkf_close(mkf);
        return 1;
    }

    error = rich4_mkf_read_resource(mkf, index, &data, &data_size, NULL);
    if (error != RICH4_MKF_OK) {
        fprintf(stderr, "error: read resource %lu failed [%s]: %s\n",
                (unsigned long)index,
                rich4_mkf_error_name(error),
                rich4_mkf_last_error(mkf));
        free(data);
        rich4_mkf_close(mkf);
        return 1;
    }

    output = fopen(argv[3], "wb");
    if (output == NULL) {
        fprintf(stderr, "error: cannot open output: %s\n", argv[3]);
        free(data);
        rich4_mkf_close(mkf);
        return 1;
    }
    if (data_size != 0 && fwrite(data, 1, data_size, output) != data_size) {
        fprintf(stderr, "error: write failed: %s\n", argv[3]);
        fclose(output);
        free(data);
        rich4_mkf_close(mkf);
        return 1;
    }
    if (fclose(output) != 0) {
        fprintf(stderr, "error: close failed: %s\n", argv[3]);
        free(data);
        rich4_mkf_close(mkf);
        return 1;
    }

    printf("extracted resource %lu: %lu bytes -> %s\n",
           (unsigned long)index,
           (unsigned long)data_size,
           argv[3]);
    free(data);
    rich4_mkf_close(mkf);
    return 0;
}
