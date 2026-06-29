#include "platform/sdl1/game_lazy_assets.h"
#include <stdio.h>
#include <string.h>

#include <SDL/SDL.h>

#include "core/game_command.h"
#include "core/character_speech_rules.h"
#include "core/property.h"
#include "data/character_speech.h"
#include "app/new_game_setup_flow.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/input_adapter.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/render/game_bank_render.h"
#include "platform/sdl1/render/game_panel_render.h"
#include "platform/sdl1/render/game_route_render.h"
#include "platform/sdl1/render/game_scene_view.h"
#include "platform/sdl1/runtime_render_api.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/game_route_api.h"
#include "resource/spr_smp.h"
#include "view/map_renderer.h"

/* R10.1c: extracted verbatim from src/platform/sdl1/runtime_main.c. The route
 * and property entrypoints are dispatched from draw_game_session;
 * draw_facility_choice_selector is also reused by the magic-house renderer. */

static void draw_pressed_region(SDL_Surface *screen,
                                int left, int top, int right, int bottom)
{
    int x;
    int y;
    if (screen == NULL || screen->format == NULL ||
        screen->format->BytesPerPixel != 2 || right <= left + 1 ||
        bottom <= top + 1)
        return;
    SDL_LockSurface(screen);
    /* sub_451B9E shifts the interior one pixel down/right, then replaces
     * the top and left edges with the pressed-state border. */
    for (y = bottom - 2; y >= top; y--) {
        uint16_t *source = (uint16_t *)
            ((unsigned char *)screen->pixels + y * screen->pitch);
        uint16_t *destination = (uint16_t *)
            ((unsigned char *)screen->pixels + (y + 1) * screen->pitch);
        for (x = right - 2; x >= left; x--)
            destination[x + 1] = source[x];
    }
    for (x = left; x < right; x++) {
        uint16_t *row = (uint16_t *)
            ((unsigned char *)screen->pixels + top * screen->pitch);
        row[x] = 0;
    }
    for (y = top; y < bottom; y++) {
        uint16_t *row = (uint16_t *)
            ((unsigned char *)screen->pixels + y * screen->pitch);
        row[left] = 0;
    }
    SDL_UnlockSurface(screen);
}

static void draw_facility_choice_selector_with_pressed(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    int hover, int pressed)
{
    static const char prompt[] =
        "\xbd\xd0\xbf\xef\xbe\xdc\xb3\x5d\xac\x49\xc3\xfe\xa7\x4f";
    static const char *const choice_name[5] = {
        "\xa4\xbd  \xb6\xe9",
        "\xae\xc8  \xc0\x5d",
        "\xc1\xca\xaa\xab\xa4\xa4\xa4\xdf",
        "\xa5\x5b\xaa\x6f\xaf\xb8",
        "\xac\xe3\xa8\x73\xa9\xd2"
    };
    Rich4SprSmpChunk chunk;
    int drew_choices = 0;
    if (screen == NULL || assets == NULL) return;
    if (game_asset_ready(assets, &assets->ui.has_worker_choice_ui) &&
        rich4_spr_smp_get_chunk(
            assets->ui.worker_choice_data, assets->ui.worker_choice_size,
            &assets->ui.worker_choice_spr, 4, &chunk) == RICH4_SPR_SMP_OK)
        drew_choices = draw_chunk_topleft(
            screen, assets->ui.worker_choice_data, &chunk, 43, 279, 1);
    if (!drew_choices) {
        SDL_LockSurface(screen);
        fill_rect(screen, 43, 279, 340, 89, 0x7bef);
        SDL_UnlockSurface(screen);
    }
    if (game_asset_ready(assets, &assets->ui.has_worker_choice_ui) &&
        rich4_spr_smp_get_chunk(
            assets->ui.worker_choice_data, assets->ui.worker_choice_size,
            &assets->ui.worker_choice_spr, 5, &chunk) == RICH4_SPR_SMP_OK)
        draw_chunk_at(
            screen, assets->ui.worker_choice_data, &chunk, 220, 140, 1);

    /* sub_440AAC/sub_43FAE4: Data[517], text at (220,122/154). */
    draw_big5_text(
        screen, 220, 122, prompt, 16,
        rich4_character_rgb565_color(0x00f0f0f0u),
        rich4_character_rgb565_color(0x00101010u), 3, 2);
    if (hover >= 0 && hover < 5) {
        uint16_t yellow =
            rich4_character_rgb565_color(0x00ffff00u);
        int left = 50 + hover * 68;
        draw_big5_text(
            screen, 220, 154, choice_name[hover], 16,
            rich4_character_rgb565_color(0x00f0f0f0u),
            rich4_character_rgb565_color(0x00101010u), 3, 4);
        SDL_LockSurface(screen);
        stroke_rect(screen, left, 286, 68, 68, yellow);
        stroke_rect(screen, left + 1, 287, 66, 66, yellow);
        stroke_rect(screen, left + 2, 288, 64, 64, yellow);
        SDL_UnlockSurface(screen);
    }
    if (pressed >= 0 && pressed < 5)
        draw_pressed_region(screen, 50 + pressed * 68, 286,
                            118 + pressed * 68, 354);
}

void draw_facility_choice_selector(
    SDL_Surface *screen, const GamePreviewAssets *assets, int hover)
{
    draw_facility_choice_selector_with_pressed(screen, assets, hover, -1);
}

static void draw_facility_research_selector_with_pressed(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    int hover, int choice_count, int pressed)
{
    Rich4SprSmpChunk chunk;
    int i;
    int drew_choices = 0;
    int drew_research_icon;
    /* 0x44101D / ref_00465298: the original Big5 prompt.  The research
     * strip is Data.MKF[0x205] (decimal 517), chunk 7; Panel.MKF[11]
     * chunks 10..14 are overlaid at the five 76-pixel cell centers. */
    static const char prompt[] =
        "\xbd\xd0\xbf\xef\xbe\xdc\xb1\xfd\xb6\x7d\xb5\x6f\xb9\x44\xa8\xe3";
    if (screen == NULL || assets == NULL) return;
    if (choice_count < 0) choice_count = 0;
    if (choice_count > 5) choice_count = 5;
    if (game_asset_ready(assets, &assets->ui.has_worker_choice_ui) &&
        rich4_spr_smp_get_chunk(
            assets->ui.worker_choice_data, assets->ui.worker_choice_size,
            &assets->ui.worker_choice_spr, 7, &chunk) == RICH4_SPR_SMP_OK)
        drew_choices = draw_chunk_topleft(
            screen, assets->ui.worker_choice_data, &chunk, 20, 280, 1);
    if (!drew_choices) {
        SDL_LockSurface(screen);
        fill_rect(screen, 20, 280, 400, 89, 0x7bef);
        SDL_UnlockSurface(screen);
    }
    draw_big5_text(
        screen, 220, 122, prompt, 16,
        rich4_character_rgb565_color(0x00f0f0f0u),
        rich4_character_rgb565_color(0x00101010u), 3, 2);
    for (i = 0; i < choice_count; i++) {
        drew_research_icon = 0;
        /* 0x44101D overlays Panel.MKF[11] chunks 10..14 on the five
         * Data.MKF[0x205] chunk-7 cells. */
        if (game_asset_ready(assets, &assets->ui.has_panel[11]))
            drew_research_icon = draw_game_panel_chunk_at(
                screen, assets, 11, (size_t)(10 + i),
                68 + i * 76, 324, 1);
        if (!drew_research_icon)
            draw_big5_text(
                screen, 68 + i * 76, 324, game_tool_big5_name(7 + i), 12,
                rich4_character_rgb565_color(0x00f0f0f0u),
                rich4_character_rgb565_color(0x00101010u), 3, 2);
    }
    if (hover >= 0 && hover < choice_count) {
        uint16_t yellow =
            rich4_character_rgb565_color(0x00ffff00u);
        int left = 32 + hover * 76;
        draw_big5_text(
            screen, 220, 154, game_tool_big5_name(7 + hover), 16,
            rich4_character_rgb565_color(0x00f0f0f0u),
            rich4_character_rgb565_color(0x00101010u), 3, 4);
        SDL_LockSurface(screen);
        stroke_rect(screen, left, 294, 71, 59, yellow);
        stroke_rect(screen, left + 1, 295, 69, 57, yellow);
        SDL_UnlockSurface(screen);
    }
    if (pressed >= 0 && pressed < choice_count)
        draw_pressed_region(screen, 35 + pressed * 76, 297,
                            101 + pressed * 76, 351);
}

void draw_facility_research_selector(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    int hover, int choice_count)
{
    draw_facility_research_selector_with_pressed(
        screen, assets, hover, choice_count, -1);
}

void draw_game_route_interaction(SDL_Surface *screen,
                                 const GamePreviewAssets *assets,
                                 const GameSceneView *scene)
{
    Rich4SprSmpChunk background;
    Rich4SprSmpChunk buttons;
    char message[160];
    int left, top;
    if (screen == NULL || assets == NULL || scene == NULL ||
        !scene->route_interaction_active)
        return;
    if (scene->route_interaction_type == GAME_COMMAND_BANK_WITHDRAW ||
        scene->route_interaction_type == GAME_COMMAND_BANK_APPLY_LOAN) {
        draw_game_bank_interaction(screen, assets, scene);
        return;
    }
    if (scene->route_interaction_facility_build) {
        draw_facility_choice_selector_with_pressed(
            screen, assets, scene->route_interaction_hover,
            scene->route_interaction_pressed);
        return;
    }
    if (scene->route_interaction_facility_research) {
        draw_facility_research_selector_with_pressed(
            screen, assets, scene->route_interaction_hover,
            scene->route_interaction_facility_choice_count,
            scene->route_interaction_pressed);
        return;
    }
    /* sub_440BA8 anchors Data.mkf[0x205].chunk_tab[5] at (220,140)
     * before drawing the question text.  Data.mkf[440] below contains only
     * the separate confirmation controls handled by sub_453A32. */
    if (game_asset_ready(assets, &assets->ui.has_common_dialog) &&
        rich4_spr_smp_get_chunk(assets->ui.common_dialog_data,
                                assets->ui.common_dialog_size,
                                &assets->ui.common_dialog_spr,
                                5,
                                &background) == RICH4_SPR_SMP_OK) {
        draw_spr_smp_center(screen, assets->ui.common_dialog_data, assets->ui.common_dialog_size,
                &assets->ui.common_dialog_spr, 5, 220, 140);
    }
    if (scene->route_interaction_type == GAME_COMMAND_PURCHASE_PROPERTY) {
        /* 0x41A013 formats one five-line string; sub_440BA8 draws the entire
         * block once at (220,140), anchor 4, using the 16 px 0xF0F0F0 font
         * with its 0x101010 one-pixel shadow. */
        snprintf(message, sizeof(message),
                 "%s\n\n\xb6\x4f\xa5\xce:%d\xa4\xb8\n\n"
                 "\xac\x4f\xa7\x5f\xb6\x52\xa4\x55\xa6\xb9\xa6\x61\xa1\x48",
                 scene->route_interaction_name,
                 scene->route_interaction_amount);
        draw_big5_text(screen, 220, 140, message, 16,
                       rich4_character_rgb565_color(0x00f0f0f0u),
                       rich4_character_rgb565_color(0x00101010u), 3, 4);
    } else if (scene->route_interaction_type == GAME_COMMAND_UPGRADE_PROPERTY) {
        /* ref_46396D is used by both the ordinary-land and large-building
         * upgrade branches; it goes through exactly the same sub_440BA8
         * renderer and confirmation controls as purchase. */
        snprintf(message, sizeof(message),
                 "%s\n\n\xa4\xc9\xaf\xc5\xb6\x4f\xa5\xce:%d\xa4\xb8\n\n"
                 "\xac\x4f\xa7\x5f\xa4\xc9\xaf\xc5\xa1\x48",
                 scene->route_interaction_name,
                 scene->route_interaction_amount);
        draw_big5_text(screen, 220, 140, message, 16,
                       rich4_character_rgb565_color(0x00f0f0f0u),
                       rich4_character_rgb565_color(0x00101010u), 3, 4);
    } else if (scene->route_interaction_type != GAME_COMMAND_CONFIRM) {
        /* Unknown interaction kinds must not reuse the purchase copy. */
        return;
    }
    if (game_confirm_dialog_bounds(assets, scene->route_interaction_type,
                                   &left, &top, NULL, NULL) &&
        rich4_spr_smp_get_chunk(assets->ui.confirm_dialog_data,
                                assets->ui.confirm_dialog_size,
                                &assets->ui.confirm_dialog_spr,
                                (size_t)(scene->route_interaction_hover ==
                                             RICH4_CONFIRM_DIALOG_ACCEPT
                                         ? 1
                                         : scene->route_interaction_hover ==
                                               RICH4_CONFIRM_DIALOG_DECLINE
                                             ? 2 : 0),
                                &buttons) == RICH4_SPR_SMP_OK) {
        draw_chunk_topleft(screen, assets->ui.confirm_dialog_data, &buttons,
                           left, top, 1);
    }
}

void draw_game_speech_overlay(SDL_Surface *screen,
                              const GamePreviewAssets *assets,
                              const GameSceneView *scene)
{
    Rich4SprSmpChunk chunk;
    if (screen == NULL || assets == NULL || scene == NULL) return;
    if (scene->speech_active && scene->speech_character >= 0) {
        int character = scene->speech_character;
        const Rich4CharacterSpeech *speech = scene->speech;
        Rich4PlayerSayText decoded;
        rich4_player_say_decode(speech != NULL ? speech->big5 : NULL,
                                speech != NULL ? speech->speaking_resource : -1,
                                &decoded);
        if (game_asset_ready(assets, &assets->ui.has_common_dialog) &&
            rich4_spr_smp_get_chunk(assets->ui.common_dialog_data,
                                    assets->ui.common_dialog_size,
                                    &assets->ui.common_dialog_spr, 6,
                                    &chunk) == RICH4_SPR_SMP_OK)
            draw_chunk_at(screen, assets->ui.common_dialog_data, &chunk,
                          220, 130, 1);
        if (character >= 0 && character < 12 &&
            assets->character.has_map_character[character])
            draw_spr_smp_center(screen,
                               assets->character.map_character_data[character],
                               assets->character.map_character_size[character],
                               &assets->character.map_character_spr[character],
                               (size_t)(scene->speech_type + 1), 170, 130);
        if (decoded.icon_chunk >= 0 && game_asset_ready(assets, &assets->ui.has_speech_icons)) {
            if (decoded.icon_chunk < (int)assets->ui.speech_icon_spr.num_chunks)
                draw_spr_smp_center(screen, assets->ui.speech_icon_data,
                                   assets->ui.speech_icon_size,
                                   &assets->ui.speech_icon_spr,
                                   (size_t)decoded.icon_chunk, 240, 130);
        } else if (decoded.display_text != NULL) {
            /* player_say/sub_44EF41 submits the complete (possibly
             * multiline) block once at anchor 5: left + vertical centre. */
            draw_big5_text(screen, 200, 130, decoded.display_text, 16,
                           rich4_character_rgb565_color(0x00101010u),
                           0, 0, 5);
        }
    }
}

static const char *god_fee_label(int id)
{
            static const char *labels[] = {
                "\xb9\x4c\xb8\xf4\xb6\x4f", "\xa9\xd0\xaf\xb2\xb6\x4f",
                "\xa5\x5b\xaa\x6f\xb6\x4f", "\xad\xd7\xa8\xae\xb6\x4f",
                "\xa9\xb1\xaf\xb2\xb6\x4f", "\xa6\xed\xb1\x4a\xb6\x4f",
                "\xae\xc8\xb9\x43\xb6\x4f", "\xab\x4f\xc0\x49\xb6\x4f",
                "\xb9\x71\xb8\xa3\xb6\x4f", "\xa4\x75\xb5\x7b\xb6\x4f",
                "\xa4\xf4\xb6\x4f", "\xb9\x71\xb6\x4f", "\xc1\xca\xaa\xab\xb6\x4f"
            };
    return labels[id >= 0 && id < 13 ? id : 0];
}

void draw_game_property_presentation(SDL_Surface *screen,
                                     const GamePreviewAssets *assets,
                                     const GameSceneView *scene)
{
    Rich4SprSmpChunk chunk;
    if (screen == NULL || assets == NULL || scene == NULL) return;
    if (scene->ai_tool_notice) {
        char message[96];
        /* 0x44805E uses 0x4653E5: "使用%s", without a player name.
         * Reuse the same 0x440CAC common-dialog chunk and text style. */
        if (game_asset_ready(assets, &assets->ui.has_common_dialog) &&
            rich4_spr_smp_get_chunk(assets->ui.common_dialog_data,
                                    assets->ui.common_dialog_size,
                                    &assets->ui.common_dialog_spr, 5,
                                    &chunk) == RICH4_SPR_SMP_OK)
            draw_spr_smp_center(screen, assets->ui.common_dialog_data, assets->ui.common_dialog_size,
                &assets->ui.common_dialog_spr, 5, 220, 140);
        snprintf(message, sizeof(message), "\xa8\xcf\xa5\xce%s",
                 game_tool_big5_name(scene->ai_tool_id));
        draw_big5_text(screen, 220, 140, message, 16,
                       rich4_character_rgb565_color(0x00f0f0f0u),
                       rich4_character_rgb565_color(0x00101010u), 3, 4);
    }
    if (scene->property_presentation_phase == GAME_PROPERTY_PRESENT_GOD_FEE ||
        scene->property_presentation_phase == GAME_PROPERTY_PRESENT_GOD_BUILD_TEXT) {
        char message[192];
        const char *format = NULL;
        const char *subject = "\xb9\x4c\xb8\xf4\xb6\x4f";
        if (scene->property_presentation_phase == GAME_PROPERTY_PRESENT_GOD_FEE) {
            /* 0x47517C: original fee labels, selected at the call site. */
            subject = god_fee_label(scene->property_fee_label);
            switch (scene->property_god_type) {
        case 1: format = "\xa4\x70\xb0\x5d\xaf\xab\xc5\xe3\xc6\x46\x0a\x0a\x25\x73\xb4\xee\xa7\x4b\xa4\x40\xa5\x62\xa1\x49"; break;
        case 2: format = "\xa4\x6a\xb0\x5d\xaf\xab\xc5\xe3\xc6\x46\x0a\x0a\xa7\x4b\xa5\x49\x25\x73\xa1\x49"; break;
        case 5: format = "\xa4\x70\xbd\x61\xaf\xab\xc5\xe3\xc6\x46\n\n%s\xa5\x5b\xa5\x49" "50\xa2\x48\xa1\x49"; break;
        case 6: format = "\xa4\x6a\xbd\x61\xaf\xab\xc5\xe3\xc6\x46\x0a\x0a\xa5\x5b\xad\xbf\xa5\x49\x25\x73\xa1\x49"; break;
            }
        } else {
            switch (scene->property_god_type) {
            case 9: format = "%s\xc5\xe3\xc6\x46\n\n\xa5\x5b\xbb\x5c\xa4\x40\xbc\x68\xa9\xd0\xab\xce\xa1\x49";
                subject = "\xa4\xd1\xa8\xcf"; break;
            case 10: format = "\xa4\x70\xb4\x63\xc5\x5d\xc5\xe3\xc6\x46\n\n\xa9\xee\xb7\xb4\xa4\x40\xbc\x68\xa9\xd0\xab\xce\xa1\x49"; break;
            case 12: format = "\xa4\x67\xa6\x61\xa4\xbd\xc5\xe3\xc6\x46\n\n\xb1\x6a\xa6\xfb\xa4\x67\xa6\x61\xa1\x49"; break;
            }
        }
        if (format != NULL) {
            snprintf(message, sizeof(message), format, subject);
            if (game_asset_ready(assets, &assets->ui.has_common_dialog) && rich4_spr_smp_get_chunk(
                    assets->ui.common_dialog_data, assets->ui.common_dialog_size,
                    &assets->ui.common_dialog_spr, 5, &chunk) == RICH4_SPR_SMP_OK)
                draw_spr_smp_center(screen, assets->ui.common_dialog_data, assets->ui.common_dialog_size,
                &assets->ui.common_dialog_spr, 5, 220, 140);
            draw_big5_text(screen, 220, 140, message, 16,
                rich4_character_rgb565_color(0x00f0f0f0u),
                rich4_character_rgb565_color(0x00101010u), 3, 4);
        }
    }
    if (scene->property_presentation_phase ==
            GAME_PROPERTY_PRESENT_BEGGAR_AMOUNT) {
        char message[96];
        if (game_asset_ready(assets, &assets->ui.has_common_dialog) &&
            rich4_spr_smp_get_chunk(assets->ui.common_dialog_data,
                                    assets->ui.common_dialog_size,
                                    &assets->ui.common_dialog_spr, 5,
                                    &chunk) == RICH4_SPR_SMP_OK)
            draw_spr_smp_center(screen, assets->ui.common_dialog_data, assets->ui.common_dialog_size,
                &assets->ui.common_dialog_spr, 5, 220, 140);
        /* ref_00463ab1: 施捨給乞丐%d元. */
        snprintf(message, sizeof(message),
                 "\xac\x49\xb1\xcb\xb5\xb9\xa4\x5e\xa4\xa2%d\xa4\xb8",
                 scene->property_presentation_amount);
        draw_big5_text(screen, 220, 140, message, 16,
                       rich4_character_rgb565_color(0x00f0f0f0u),
                       rich4_character_rgb565_color(0x00101010u), 3, 4);
    } else if (scene->property_presentation_phase ==
                   GAME_PROPERTY_PRESENT_GOD_TRADE_REJECTION) {
        const char *god_name = NULL;
        char message[96];
        /* Full NUL-terminated format at 0x463514; IDA's automatic string
         * label displayed only the ASCII prefix "%s". */
        switch (scene->property_presentation_amount) {
        case 7:
            god_name = "\xa4\x70\xb0\x49\xaf\xab";
            break;
        case 8:
            god_name = "\xa4\x6a\xb0\x49\xaf\xab";
            break;
        case 15:
            god_name = "\xa6\xba\xaf\xab";
            break;
        default:
            break;
        }
        if (game_asset_ready(assets, &assets->ui.has_common_dialog) &&
            rich4_spr_smp_get_chunk(assets->ui.common_dialog_data,
                                    assets->ui.common_dialog_size,
                                    &assets->ui.common_dialog_spr, 5,
                                    &chunk) == RICH4_SPR_SMP_OK)
            draw_spr_smp_center(screen, assets->ui.common_dialog_data, assets->ui.common_dialog_size,
                &assets->ui.common_dialog_spr, 5, 220, 140);
        if (god_name != NULL) {
            snprintf(message, sizeof(message), "%s\xc5\xe3\xc6\x46\n\n\xa7\xeb\xb8\xea\xa5\xa2\xb1\xd1\xa1\x49", god_name);
            draw_big5_text(screen, 220, 140, message, 16,
                           rich4_character_rgb565_color(0x00f0f0f0u),
                           rich4_character_rgb565_color(0x00101010u), 3, 4);
        }
    } else if (scene->property_presentation_phase ==
                   GAME_PROPERTY_PRESENT_AMOUNT) {
        char message[192];
        if (game_asset_ready(assets, &assets->ui.has_common_dialog) &&
            rich4_spr_smp_get_chunk(assets->ui.common_dialog_data,
                                    assets->ui.common_dialog_size,
                                    &assets->ui.common_dialog_spr, 5,
                                    &chunk) == RICH4_SPR_SMP_OK)
            draw_spr_smp_center(screen, assets->ui.common_dialog_data, assets->ui.common_dialog_size,
                &assets->ui.common_dialog_spr, 5, 220, 140);
        if (scene->property_facility_type == 1 && scene->property_outcome > 0) {
            /* 0x4639FF, formatted at 0x41A478 before payment/entry. */
            snprintf(message, sizeof(message),
                "\xa5\xf0\xae\xa7%d\xa4\xd1\n\n\xb6\x4f\xa5\xce%d\xa4\xb8\xa1\x49",
                scene->property_outcome, scene->property_presentation_amount);
        } else if (scene->property_company_type >= 0 || scene->property_facility_type == 3) {
            /* 0x463A31 / 0x463A6A, selected by 0x41AE79. */
            char owner_name[64];
            const char *name = scene->property_owner_character >= 0 ?
                new_game_character_name(scene->property_owner_character) : "";
            size_t length = 0;
            /* 0x41AE57 calls 0x452946 before formatting the chairman. */
            while (*name && length + 1 < sizeof(owner_name)) {
                if (*name != ' ') owner_name[length++] = *name;
                name++;
            }
            owner_name[length] = '\0';
            const char *format = scene->property_company_type == 12 ?
                "%s\n\n\xc0\xb0\xa5\x44%s\n\n\xbd\xd0\xa5\x49%d\xa4\xb8%s" :
                "%s\n\n\xb8\xb3\xa8\xc6\xaa\xf8%s\n\n\xbd\xd0\xa5\x49%d\xa4\xb8%s";
            snprintf(message, sizeof(message), format,
                scene->property_facility_type == 3 ? "\xa5\x5b\xaa\x6f\xaf\xb8" : scene->property_name,
                owner_name,
                scene->property_presentation_amount, god_fee_label(scene->property_fee_label));
        } else if (scene->property_related_owner_character >= 0 &&
            scene->property_related_amount > 0) {
            snprintf(message, sizeof(message),
                     /* 0x419D1A formats with 0x46399A and fee table 0x47517C. */
                     "%s\n\n\xc4\xdd%s\xbb\x50%s\n\n"
                     "\xbd\xd0\xa5\x49%d\xa4\xb8%s",
                     scene->property_name,
                     scene->property_owner_character >= 0
                         ? new_game_character_name(
                               scene->property_owner_character) : "",
                     new_game_character_name(
                         scene->property_related_owner_character),
                     scene->property_presentation_amount,
                     god_fee_label(scene->property_fee_label));
        } else {
            snprintf(message, sizeof(message),
                     "%s\n\n\xa6\xb9\xa6\x61\xc4\xdd%s\n\n"
                     "\xbd\xd0\xa5\x49%d\xa4\xb8",
                     scene->property_name,
                     scene->property_owner_character >= 0
                         ? new_game_character_name(scene->property_owner_character)
                         : "",
                     scene->property_presentation_amount);
        }
        /* 0x419D50 -> 0x440CAC creates a 16px 0xF0F0F0 font with a
         * 0x101010 one-pixel shadow and draws the formatted block once. */
        draw_big5_text(screen, 220, 140, message, 16,
                       rich4_character_rgb565_color(0x00f0f0f0u),
                       rich4_character_rgb565_color(0x00101010u), 3, 4);
    }
    if (scene->property_presentation_phase ==
            GAME_PROPERTY_PRESENT_BLOCKED_TEXT) {
        static const char *const formats[] = {
            NULL,
            "\xbb\x50%s\xa6\x50\xb7\xf9\xa4\xa4\x0a\x0a\xa7\x4b\xa6\xac%s\xa1\x49",
            "\xa6\xba\xaf\xab\xc5\xe3\xc6\x46\x0a\x0a\xa7\x4b\xa6\xac%s\xa1\x49",
            "%s\xa6\xed\xb1\x4a\xa4\xa4\x0a\x0a\xa7\x4b\xa6\xac%s\xa1\x49",
            "%s\xae\xf8\xa5\xa2\xa4\xa4\x0a\x0a\xa7\x4b\xa6\xac%s\xa1\x49",
            "%s\xa7\xa4\xa8\x63\xa4\xa4\x0a\x0a\xa7\x4b\xa6\xac%s\xa1\x49",
            "%s\xa6\xed\xb0\x7c\xa4\xa4\x0a\x0a\xa7\x4b\xa6\xac%s\xa1\x49",
            "%s\xa5\x56\xaf\x76\xa4\xa4\x0a\x0a\xa7\x4b\xa6\xac%s\xa1\x49",
            "%s\xb9\xda\xb9\x43\xa4\xa4\x0a\x0a\xa7\x4b\xa6\xac%s\xa1\x49",
            "\xa9\xd0\xab\xce\xac\x64\xab\xca\xa4\xa4\x0a\x0a\xa7\x4b\xa6\xac%s\xa1\x49"
        };
        static const char toll[] = "\xb9\x4c\xb8\xf4\xb6\x4f";
        char message[160];
        int reason = scene->property_collection_block_reason;
        const char *owner = scene->property_owner_character >= 0
            ? new_game_character_name(scene->property_owner_character) : "";
        if (game_asset_ready(assets, &assets->ui.has_common_dialog) &&
            rich4_spr_smp_get_chunk(assets->ui.common_dialog_data,
                                    assets->ui.common_dialog_size,
                                    &assets->ui.common_dialog_spr, 5,
                                    &chunk) == RICH4_SPR_SMP_OK)
            draw_spr_smp_center(screen, assets->ui.common_dialog_data, assets->ui.common_dialog_size,
                &assets->ui.common_dialog_spr, 5, 220, 140);
        if (reason > GAME_PROPERTY_COLLECTION_BLOCK_NONE &&
            reason <= GAME_PROPERTY_COLLECTION_BLOCK_SEALED &&
            formats[reason] != NULL) {
            if (reason == GAME_PROPERTY_COLLECTION_BLOCK_GOD ||
                reason == GAME_PROPERTY_COLLECTION_BLOCK_SEALED)
                snprintf(message, sizeof(message), formats[reason], toll);
            else
                snprintf(message, sizeof(message), formats[reason],
                         owner, toll);
            draw_big5_text(
                screen, 220, 140, message, 16,
                rich4_character_rgb565_color(0x00f0f0f0u),
                rich4_character_rgb565_color(0x00101010u), 3, 4);
        }
    }
    draw_game_speech_overlay(screen, assets, scene);
}
