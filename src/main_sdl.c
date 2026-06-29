#include "platform/sdl1/runtime.h"
#include <stdio.h>

int main(int argc, char **argv)
{
    /* Keep redirected launcher logs useful while the game is still running. */
    setvbuf(stdout, NULL, _IOLBF, 0);
    return rich4_sdl1_main(argc, argv);
}
