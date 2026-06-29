#include <assert.h>
#include <stdio.h>

#include "app/game_session.h"
#include "platform/sdl1/render/game_render_cache.h"

#ifdef main
#undef main
#endif

int main(void)
{
    assert(sizeof(GameRenderCache) > 0);
    assert(sizeof(GameSession) > sizeof(GameState));
    puts("SDL1 session header standalone include: ok");
    return 0;
}
