#ifndef RICH4_RESOURCE_FLC_ANIMATION_H
#define RICH4_RESOURCE_FLC_ANIMATION_H

#include <stddef.h>
#include <stdint.h>
#include "api_contract.h"

/* Autodesk Animator FLC file magic at header offset +4. */
#define RICH4_FLC_MAGIC 0xAF12u
#define RICH4_FLC_FRAME_MAGIC 0xF1FAu
#define RICH4_FLC_HEADER_SIZE 128u
#define RICH4_FLC_FRAME_HEADER_SIZE 16u
#define RICH4_FLC_CHUNK_HEADER_SIZE 6u

typedef enum Rich4FlcError {
    RICH4_FLC_OK = 0,
    RICH4_FLC_DONE,
    RICH4_FLC_ERR_ARGUMENT,
    RICH4_FLC_ERR_MAGIC,
    RICH4_FLC_ERR_TRUNCATED,
    RICH4_FLC_ERR_RANGE,
    RICH4_FLC_ERR_UNSUPPORTED_OPCODE
} Rich4FlcError;

typedef struct Rich4FlcHeader {
    uint32_t file_size;
    uint16_t frame_count;
    uint16_t width;
    uint16_t height;
    uint16_t depth;
    uint16_t flags;
    uint32_t delay_ms;
} Rich4FlcHeader;

typedef struct Rich4FlcFrame {
    size_t offset;
    uint32_t size;
    uint16_t command_count;
} Rich4FlcFrame;

typedef struct Rich4FlcDecoder {
    RICH4_BORROWED_LIFETIME const unsigned char *data;
    size_t size;
    Rich4FlcHeader header;
    size_t next_frame_offset;
    uint16_t next_frame_index;
    uint16_t palette[256];
    RICH4_CALLER_STORAGE uint16_t *pixels;
    size_t pixel_capacity;
    unsigned char *index_pixels;
    size_t index_capacity;
    unsigned char *dirty_pixels;
    size_t dirty_capacity;
    uint16_t last_skipped_opcode;
    unsigned int skipped_chunk_count;
} Rich4FlcDecoder;

Rich4FlcError rich4_flc_parse_header(const unsigned char *data, size_t size,
                                     Rich4FlcHeader *out_header);
Rich4FlcError rich4_flc_decoder_init(Rich4FlcDecoder *decoder,
                                     const unsigned char *data, size_t size,
                                     uint16_t *pixels, size_t pixel_capacity);
/* Seed the decoder's caller-owned RGB565 framebuffer before decoding a
 * delta frame. The source is copied immediately and is not retained. */
void rich4_flc_decoder_set_source(Rich4FlcDecoder *decoder,
                                  const uint16_t *source_pixels,
                                  size_t source_capacity);
void rich4_flc_decoder_set_indices(Rich4FlcDecoder *decoder,
                                   unsigned char *index_pixels,
                                   size_t index_capacity);
void rich4_flc_decoder_set_dirty(Rich4FlcDecoder *decoder,
                                 unsigned char *dirty_pixels,
                                 size_t dirty_capacity);
Rich4FlcError rich4_flc_decode_next(Rich4FlcDecoder *decoder,
                                    Rich4FlcFrame *out_frame);
Rich4FlcError rich4_flc_decode_next_ex(Rich4FlcDecoder *decoder,
                                       Rich4FlcFrame *out_frame,
                                       int decode_pixels);
const char *rich4_flc_error_name(Rich4FlcError error);

#endif
