#include <assert.h>
#include <stdio.h>

#include "api_contract.h"
#include "app/audio_event.h"
#include "app/scene.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "core/game_command.h"
#include "core/game_event.h"
#include "core/game_dispatch.h"
#include "core/new_game_init.h"
#include "core/player_status.h"
#include "core/route_event_queue.h"
#include "core/route_movement.h"
#include "core/status_countdown.h"
#include "core/turn_rotation.h"
#include "core/victory.h"
#include "data/map_data.h"
#include "data/map_object_ref.h"
#include "data/resource_catalog.h"
#include "debug/trace.h"
#include "resource/mkf.h"
#include "data/music.h"
#include "platform/sdl1/input_adapter.h"
#include "platform/sdl1/runtime.h"
#include "resource/flc_animation.h"
#include "resource/flc_player.h"
#include "resource/resource_store.h"
#include "resource/spr_smp.h"
#include "view/flc_compositor.h"
#include "view/game_view_model.h"
#include "view/map_renderer.h"

int main(void)
{
    assert(RICH4_API_CONTRACT_VERSION == 1);
    rich4_mkf_close(NULL);
    rich4_resource_store_destroy(NULL);
    rich4_map_data_free(NULL);
    assert(rich4_resource_handle_is_valid(NULL) == 0);
    assert(rich4_mkf_count(NULL) == 0);
    puts("public API contract headers: ok");
    return 0;
}
