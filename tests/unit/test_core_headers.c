#include "core/game_constants.h"
#include "core/ai.h"
#include "core/game_command.h"
#include "core/game_domain.h"
#include "core/game_event.h"
#include "core/ledger.h"
#include "core/game_dispatch.h"
#include "core/first_turn_dispatch.h"
#include "core/facility.h"
#include "core/card.h"
#include "core/confinement.h"
#include "core/god.h"
#include "core/game_flow_state.h"
#include "core/game_types.h"
#include "core/game_state.h"
#include "core/new_game_init.h"
#include "core/player_status.h"
#include "core/property.h"
#include "core/rng.h"
#include "core/route_movement.h"
#include "core/route_event_queue.h"
#include "core/status_countdown.h"
#include "core/stock.h"
#include "core/special_event.h"
#include "core/tool.h"
#include "core/turn_rotation.h"
#include "core/periodic_finance.h"
#include "core/victory.h"

#include <stdio.h>

#ifdef _SDL_H
#error core headers must not include SDL
#endif

int main(void)
{
    GamePlayerState player = {0};
    GameStockInfo stock = {0};
    GameCharacterTemplate character = {0};
    GameState state = {0};

    player.character = 1;
    stock.f4 = 2;
    character.color = 3;
    state.players[0] = player;
    if (player.character != 1 || stock.f4 != 2 || character.color != 3 ||
        state.players[0].character != 1 || GAME_MAX_PLAYERS != 4) {
        return 1;
    }
    puts("core headers without SDL: ok");
    return 0;
}
