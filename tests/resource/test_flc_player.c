#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "resource/flc_player.h"
#include "view/flc_compositor.h"

static void put16(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8);
}

static void put32(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16); p[3] = (unsigned char)(v >> 24);
}

static size_t make_delta_flc(unsigned char data[256])
{
    size_t frame0 = 128;
    size_t chunk = frame0 + 16;
    size_t frame1 = frame0 + 44;
    memset(data, 0, 256);
    put32(data, 202); put16(data + 4, RICH4_FLC_MAGIC); put16(data + 6, 2);
    put16(data + 8, 3); put16(data + 10, 1); put16(data + 12, 8); put32(data + 16, 77);
    put32(data + frame0, 44); put16(data + frame0 + 4, RICH4_FLC_FRAME_MAGIC);
    put16(data + frame0 + 6, 2);
    put32(data + chunk, 19); put16(data + chunk + 4, 4);
    put16(data + chunk + 6, 1); data[chunk + 8] = 0; data[chunk + 9] = 3;
    data[chunk + 10] = 0; data[chunk + 11] = 0; data[chunk + 12] = 0;
    data[chunk + 13] = 255; data[chunk + 14] = 0; data[chunk + 15] = 0;
    data[chunk + 16] = 0; data[chunk + 17] = 255; data[chunk + 18] = 0;
    chunk += 19;
    put32(data + chunk, 9); put16(data + chunk + 4, 16);
    data[chunk + 6] = 1; data[chunk + 7] = 1; data[chunk + 8] = 1;
    put32(data + frame1, 30); put16(data + frame1 + 4, RICH4_FLC_FRAME_MAGIC);
    put16(data + frame1 + 6, 1);
    chunk = frame1 + 16;
    put32(data + chunk, 14); put16(data + chunk + 4, 12);
    put16(data + chunk + 6, 0); put16(data + chunk + 8, 1);
    data[chunk + 10] = 1; data[chunk + 11] = 1;
    data[chunk + 12] = 1; data[chunk + 13] = 2;
    return 202;
}

int main(void)
{
    unsigned char data[256];
    uint16_t pixels[3], source[3] = {9,8,7}, output[3];
    unsigned char indices[3], dirty[3];
    Rich4FlcPlayer player;
    int advanced;
    size_t size = make_delta_flc(data);
    assert(rich4_flc_player_init(&player, data, size, pixels, indices, dirty, 3, 0) == RICH4_FLC_OK);
    assert(player.delay_ms == 77);
    assert(rich4_flc_player_decode_to(&player, 0, &advanced) == RICH4_FLC_OK && advanced == 1);
    assert(indices[0] == 1 && indices[1] == 1);
    assert(rich4_flc_player_decode_to(&player, 0, &advanced) == RICH4_FLC_OK && advanced == 0);
    assert(rich4_flc_player_decode_to(&player, 1, &advanced) == RICH4_FLC_OK && advanced == 1);
    assert(indices[1] == 2);
    assert(rich4_flc_player_decode_to(&player, 0, &advanced) == RICH4_FLC_OK && advanced == 1);
    assert(indices[1] == 1 && pixels[1] == 0xf800);
    assert(rich4_flc_player_decode_to(&player, -1, NULL) == RICH4_FLC_ERR_RANGE);
    assert(rich4_flc_player_seek(&player, 1) == RICH4_FLC_OK);
    assert(indices[0] == 1 && indices[1] == 2 && indices[2] == 1);
    assert(pixels[0] == 0xf800 && pixels[1] == 0x07e0 && pixels[2] == 0xf800);
    assert(rich4_flc_player_seek(&player, 1) == RICH4_FLC_OK);
    assert(indices[1] == 2);
    assert(rich4_flc_player_init(&player, data, size, pixels, indices, dirty, 3, 10) == RICH4_FLC_OK);
    assert(player.delay_ms == 10);
    assert(rich4_flc_player_advance(&player, 100, &advanced) == RICH4_FLC_OK && advanced);
    assert(rich4_flc_player_advance(&player, 105, &advanced) == RICH4_FLC_OK && !advanced);
    assert(rich4_flc_player_advance(&player, 110, &advanced) == RICH4_FLC_OK && advanced);
    indices[0] = 0; indices[1] = 1; indices[2] = 0;
    pixels[0] = 0; pixels[1] = 0xf800; pixels[2] = 0;
    rich4_flc_composite_game(indices, pixels, source, output, 3, 1);
    assert(output[0] == 9 && output[1] == 0xf800 && output[2] == 7);
    puts("flc player/compositor: ok");
    return 0;
}
