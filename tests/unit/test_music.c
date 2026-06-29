#include <assert.h>
#include <stdio.h>

#include "data/music.h"

int main(void)
{
    assert(rich4_music_point_minigame_index(6) == 12);
    assert(rich4_music_point_minigame_index(7) == 11);
    assert(rich4_music_point_minigame_index(8) == 10);
    assert(rich4_music_point_minigame_index(5) == -1);
    assert(rich4_music_point_minigame_index(9) == -1);
    puts("point minigame music mapping: ok");
    return 0;
}
