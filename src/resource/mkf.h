#ifndef RICH4_SDL_MKF_H
#define RICH4_SDL_MKF_H

#include <stddef.h>
#include <stdint.h>
#include "api_contract.h"

typedef enum Rich4MkfError {
    RICH4_MKF_OK = 0,
    RICH4_MKF_ERR_IO,
    RICH4_MKF_ERR_FORMAT,
    RICH4_MKF_ERR_RANGE,
    RICH4_MKF_ERR_MEMORY,
    RICH4_MKF_ERR_DECOMPRESS
} Rich4MkfError;

typedef struct Rich4Mkf Rich4Mkf;

typedef struct Rich4MkfResourceInfo {
    uint32_t offset;
    uint32_t uncompressed_size;
    uint32_t compressed_size;
    uint32_t image_data_offset;
    uint32_t image_data_size;
} Rich4MkfResourceInfo;

/* OWNED on success. Historical exception: failures after handle allocation
 * return an owned diagnostic handle so rich4_mkf_last_error remains usable. */
Rich4MkfError rich4_mkf_open(RICH4_BORROWED const char *path,
                             RICH4_OWNED Rich4Mkf **out_mkf);
RICH4_NULL_SAFE void rich4_mkf_close(Rich4Mkf *mkf);
const char *rich4_mkf_last_error(const Rich4Mkf *mkf);
size_t rich4_mkf_count(const Rich4Mkf *mkf);
Rich4MkfError rich4_mkf_get_info(const Rich4Mkf *mkf, size_t index,
                                 Rich4MkfResourceInfo *out_info);
Rich4MkfError rich4_mkf_read_resource(Rich4Mkf *mkf, size_t index,
                                      RICH4_OWNED unsigned char **out_data,
                                      size_t *out_size,
                                      RICH4_OPTIONAL_OUT Rich4MkfResourceInfo *out_info);
const char *rich4_mkf_error_name(Rich4MkfError error);

#endif

