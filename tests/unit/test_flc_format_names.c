#include "resource/flc_animation.h"

#include <stdio.h>
#include <string.h>

static int file_contains(const char *path, const char *needle)
{
    FILE *fp = fopen(path, "rb");
    char line[512];
    if (fp == NULL) return 1;
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strstr(line, needle) != NULL) {
            fclose(fp);
            return 1;
        }
    }
    fclose(fp);
    return 0;
}

int main(void)
{
    Rich4FlcHeader header = {0};
    Rich4FlcFrame frame = {0, 0, 0};
    Rich4FlcDecoder decoder = {0};

    if (RICH4_FLC_MAGIC != 44818u ||
        RICH4_FLC_FRAME_MAGIC != 0xF1FAu ||
        RICH4_FLC_HEADER_SIZE != 128u ||
        RICH4_FLC_FRAME_HEADER_SIZE != 16u ||
        header.frame_count != 0 || frame.command_count != 0 ||
        decoder.data != NULL ||
        file_contains("Makefile", "mkf_dump_af12_frames") ||
        file_contains("tools/mkf_dump_flc_frames.c", "Af12")) {
        return 1;
    }
    puts("Autodesk Animator FLC names: ok");
    return 0;
}
