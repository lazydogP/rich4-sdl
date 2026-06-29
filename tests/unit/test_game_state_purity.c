#include <stdio.h>
#include <string.h>

#include "core/game_flow_state.h"
#include "core/game_state.h"
#include "app/game_presentation.h"

static int source_contains(const char *path, const char *needle)
{
    FILE *file = fopen(path, "rb");
    char buffer[8192];
    size_t size;
    int failed;
    if (file == NULL) return 1;
    size = fread(buffer, 1, sizeof(buffer) - 1, file);
    failed = ferror(file) != 0;
    fclose(file);
    buffer[size] = '\0';
    return failed || strstr(buffer, needle) != NULL;
}

int main(void)
{
    static const char *forbidden[] = {
        "int direction;", "int walk_frame;", "int visual_x;", "int visual_y;",
        "int move_active;", "int move_from_node;", "int move_to_node;",
        "int move_from_x;", "int move_from_y;", "int move_to_x;", "int move_to_y;",
        "int move_progress;", "int move_distance;", "int intro_visible;",
        "int first_turn_landing_pending;", "int dirty_flags;",
        "uint32_t template_color;", "int template_sex;", "int template_f22;",
        "int template_f23;", "int template_f24;", "int initial_cash_ratio;",
        "int template_f26;", "int template_status_byte;", "int who_plays;"
    };
    GameState state;
    GamePresentationState flow;
    size_t i;
    memset(&state, 0, sizeof(state));
    memset(&flow, 0, sizeof(flow));
    flow.actors[0].visual_x = 123;
    flow.actors[0].move_active = 1;
    if (state.players[0].node != 0 || flow.actors[0].visual_x != 123 ||
        flow.actors[0].move_active != 1) return 1;
    for (i = 0; i < sizeof(forbidden) / sizeof(forbidden[0]); i++) {
        if (source_contains("src/core/game_types.h", forbidden[i])) {
            fprintf(stderr, "transient actor field returned to GamePlayerState: %s\n",
                    forbidden[i]);
            return 1;
        }
    }
    puts("GameState excludes transient actor presentation: ok");
    return 0;
}
