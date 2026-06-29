#ifndef RICH4_RESOURCE_SPR_SMP_H
#define RICH4_RESOURCE_SPR_SMP_H

#include <stddef.h>
#include <stdint.h>
#include "api_contract.h"

typedef enum Rich4SprSmpError {
    RICH4_SPR_SMP_OK = 0,
    RICH4_SPR_SMP_ERR_FORMAT,
    RICH4_SPR_SMP_ERR_RANGE
} Rich4SprSmpError;

typedef struct Rich4SprSmpInfo {
    char signature[4];
    uint32_t num_chunks;
    uint32_t start_offset;
} Rich4SprSmpInfo;

typedef struct Rich4SprSmpChunk {
    int16_t width;
    int16_t height;
    int16_t x;
    int16_t y;
    uint32_t data_size;
    uint32_t data_offset;
} Rich4SprSmpChunk;

int rich4_spr_smp_probe(const unsigned char *data, size_t size,
                        Rich4SprSmpInfo *out_info);
Rich4SprSmpError rich4_spr_smp_get_chunk(const unsigned char *data,
                                         size_t size,
                                         const Rich4SprSmpInfo *info,
                                         size_t index,
                                         Rich4SprSmpChunk *out_chunk);
Rich4SprSmpError rich4_spr_smp_decode_rgb555(const unsigned char *data,
                                             size_t size,
                                             const Rich4SprSmpInfo *info,
                                             const Rich4SprSmpChunk *chunk,
                                             uint16_t *out_pixels,
                                             size_t pixel_capacity);
const char *rich4_spr_smp_error_name(Rich4SprSmpError error);

#endif
