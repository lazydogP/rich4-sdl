/* R10.1d: runtime audio initialization and archive-backed sound loading
 * moved verbatim out of runtime_main.c. */

#include <stdio.h>

#include "platform/sdl1/runtime_audio_bootstrap.h"

void game_init_runtime_audio(RuntimeAudioBootstrapContext *context)
{
    Rich4Audio *audio = NULL;

    if (rich4_audio_init(&audio)) {
        rich4_audio_set_speaking_archive(audio, context->speaking_mkf);
        if (!rich4_audio_load_button_sounds(audio, context->effect_mkf)) {
            printf("button/transition sounds not available from Effect.mkf resources 0, 1, 2, 3, 4, 5, 7 and 9\n");
        }
        if (context->effect_mkf == NULL ||
            !rich4_audio_load_movement_sounds(audio, context->effect_mkf)) {
            printf("movement sounds not available from Effect.mkf\n");
        }
        if (context->effect_mkf == NULL ||
            !rich4_audio_load_dice_sound(audio, context->effect_mkf)) {
            printf("dice sound not available from Effect.mkf\n");
        }
        if (context->effect_mkf == NULL ||
            !rich4_audio_load_jump_sound(audio, context->effect_mkf)) {
            printf("jump sound not available from Effect.mkf\n");
        }
        if (context->effect_mkf == NULL ||
            !rich4_audio_load_door_sound(audio, context->effect_mkf)) {
            printf("door sound not available from Effect.mkf\n");
        }
        if (context->effect_mkf == NULL ||
            !rich4_audio_load_property_sounds(audio, context->effect_mkf)) {
            printf("property sounds not available from Effect.mkf\n");
        }
        if (context->effect_mkf == NULL ||
            !rich4_audio_load_card_sound(audio, context->effect_mkf)) {
            printf("card sound not available from Effect.mkf resource 62\n");
        }
        if (context->effect_mkf == NULL ||
            !rich4_audio_load_god_effect_sounds(audio, context->effect_mkf)) {
            printf("god-effect sounds not available from Effect.mkf resources 0x66-0x71\n");
        }
        if (context->effect_mkf == NULL ||
            !rich4_audio_load_special_place_sound(audio, context->effect_mkf)) {
            printf("special-place sounds not available from Effect.mkf resources 43 and 48\n");
        }
        if (context->effect_mkf == NULL ||
            !rich4_audio_load_tool_sounds(audio, context->effect_mkf)) {
            printf("tool placement sounds not available from Effect.mkf\n");
        }
    } else {
        printf("audio device not available; continuing silently\n");
    }
    *context->audio = audio;
}
