#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "resource/mkf.h"
#include "resource/spr_smp.h"

typedef struct DumpOptions {
    const char *path;
    int summary;
    size_t limit;
    int has_limit;
} DumpOptions;

static void print_usage(FILE *out)
{
    fprintf(out, "usage: mkf_dump [--summary] [--limit N] <path-to-mkf>\n");
}

static int print_error(const char *action, Rich4Mkf *mkf, Rich4MkfError err)
{
    fprintf(stderr, "error: %s failed [%s]: %s\n",
            action, rich4_mkf_error_name(err), rich4_mkf_last_error(mkf));
    return 1;
}

static int parse_size_arg(const char *text, size_t *out_value)
{
    char *end = NULL;
    unsigned long value;

    if (text == NULL || *text == '\0' || out_value == NULL) {
        return 0;
    }
    value = strtoul(text, &end, 10);
    if (*end != '\0') {
        return 0;
    }
    *out_value = (size_t)value;
    return 1;
}

static int parse_options(int argc, char **argv, DumpOptions *out_options)
{
    int i;

    memset(out_options, 0, sizeof(*out_options));
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--summary") == 0) {
            out_options->summary = 1;
        } else if (strcmp(argv[i], "--limit") == 0) {
            if (i + 1 >= argc || !parse_size_arg(argv[i + 1], &out_options->limit)) {
                fprintf(stderr, "error: --limit requires a non-negative integer\n");
                return 0;
            }
            out_options->has_limit = 1;
            i++;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(stdout);
            exit(0);
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "error: unknown option: %s\n", argv[i]);
            return 0;
        } else if (out_options->path == NULL) {
            out_options->path = argv[i];
        } else {
            fprintf(stderr, "error: multiple MKF paths were provided\n");
            return 0;
        }
    }

    if (out_options->path == NULL) {
        return 0;
    }
    return 1;
}

int main(int argc, char **argv)
{
    DumpOptions options;
    Rich4Mkf *mkf = NULL;
    Rich4MkfError err;
    size_t count;
    size_t printed_count;
    size_t i;

    if (!parse_options(argc, argv, &options)) {
        print_usage(stderr);
        return 2;
    }

    printf("path: %s\n", options.path);
    err = rich4_mkf_open(options.path, &mkf);
    if (err != RICH4_MKF_OK) {
        int rc = print_error("open", mkf, err);
        rich4_mkf_close(mkf);
        return rc;
    }

    count = rich4_mkf_count(mkf);
    printf("valid: yes\n");
    printf("resources: %lu\n", (unsigned long)count);

    printed_count = count;
    if (options.has_limit && options.limit < printed_count) {
        printed_count = options.limit;
    }
    if (options.has_limit) {
        printf("limit: %lu\n", (unsigned long)options.limit);
    }
    if (options.summary) {
        printf("summary: yes\n");
    }

    for (i = 0; i < printed_count; i++) {
        Rich4MkfResourceInfo info;
        unsigned char *data = NULL;
        size_t data_size = 0;
        Rich4SprSmpInfo spr;

        err = rich4_mkf_get_info(mkf, i, &info);
        if (err != RICH4_MKF_OK) {
            int rc = print_error("read resource header", mkf, err);
            rich4_mkf_close(mkf);
            return rc;
        }

        printf("resource %lu:\n", (unsigned long)i);
        printf("  offset: %lu\n", (unsigned long)info.offset);
        printf("  uncompressed_size: %lu\n", (unsigned long)info.uncompressed_size);
        printf("  compressed_size: %lu\n", (unsigned long)info.compressed_size);
        printf("  image_data_offset: %lu\n", (unsigned long)info.image_data_offset);
        printf("  image_data_size: %lu\n", (unsigned long)info.image_data_size);

        if (options.summary) {
            continue;
        }

        err = rich4_mkf_read_resource(mkf, i, &data, &data_size, NULL);
        if (err != RICH4_MKF_OK) {
            int rc = print_error("read resource data", mkf, err);
            free(data);
            rich4_mkf_close(mkf);
            return rc;
        }

        if (rich4_spr_smp_probe(data, data_size, &spr)) {
            size_t j;
            printf("  spr_smp:\n");
            printf("    signature: %.3s\n", spr.signature);
            printf("    chunks: %lu\n", (unsigned long)spr.num_chunks);
            printf("    start_offset: %lu\n", (unsigned long)spr.start_offset);
            for (j = 0; j < spr.num_chunks; j++) {
                Rich4SprSmpChunk chunk;
                Rich4SprSmpError spr_err =
                    rich4_spr_smp_get_chunk(data, data_size, &spr, j, &chunk);
                if (spr_err != RICH4_SPR_SMP_OK) {
                    fprintf(stderr,
                            "error: parse SPR/SMP chunk %lu failed [%s]\n",
                            (unsigned long)j, rich4_spr_smp_error_name(spr_err));
                    free(data);
                    rich4_mkf_close(mkf);
                    return 1;
                }
                printf("    chunk %lu: width=%d height=%d x=%d y=%d data_size=%lu data_offset=%lu\n",
                       (unsigned long)j,
                       chunk.width,
                       chunk.height,
                       chunk.x,
                       chunk.y,
                       (unsigned long)chunk.data_size,
                       (unsigned long)chunk.data_offset);
            }
        }
        free(data);
    }

    if (printed_count < count) {
        printf("truncated: printed %lu of %lu resources\n",
               (unsigned long)printed_count,
               (unsigned long)count);
    }

    rich4_mkf_close(mkf);
    return 0;
}
