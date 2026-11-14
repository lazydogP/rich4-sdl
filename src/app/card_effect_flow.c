#include "app/card_effect_flow.h"
#include "app/property_flow.h"

void rich4_card_effect_prepare(GameSession *session)
{
    GameCardUsePresentationState *use = &session->presentation.card_use;
    const GameState *s = &session->state;
    int card = use->command.subject.id, id = use->command.target.id;
    int facility = use->command.target.kind == GAME_DOMAIN_FACILITY;
    int i, source = use->command.value;
    use->exchange_caster = use->exchange_victim = -1;
    if ((use->command.target.kind != GAME_DOMAIN_LAND && !facility) || id < 0 ||
        id >= (facility ? s->facility_count : s->land_count)) return;
    if (card == 3 || card == 4) {
        int source_owner, target_owner, source_level, target_level;
        if (source < 0 || source >= (facility ? s->facility_count : s->land_count)) return;
        source_owner = facility ? s->facility_owner[source] : s->land_owner[source];
        target_owner = facility ? s->facility_owner[id] : s->land_owner[id];
        source_level = facility ? s->facility_level[source] : s->land_level[source];
        target_level = facility ? s->facility_level[id] : s->land_level[id];
        /* 0x4426F7/0x442BDF: only the owner receiving an equal/better
         * building speaks. The other owner then uses role 2, type 2. */
        if (source_owner == use->command.player && target_owner != source_owner &&
            target_level >= source_level) {
            use->exchange_caster = source_owner;
            use->exchange_victim = target_owner;
        } else if (target_owner == use->command.player && source_owner != target_owner &&
                   source_level >= target_level) {
            use->exchange_caster = target_owner;
            use->exchange_victim = source_owner;
        }
        if (facility) use->highlight_facility[id] = use->highlight_facility[source] = 1;
        else use->highlight_land[id] = use->highlight_land[source] = 1;
        use->flash_pending = 1;
    } else if (card == 8 || card == 9) {
        use->flash_pending = 1;
        if (facility) {
            use->highlight_facility[id] = 1;
            use->max_level_pending = card == 8 && s->facility_level[id] == 4 &&
                (s->facility_type[id] == 1 || s->facility_type[id] == 2 || s->facility_type[id] == 4);
        } else for (i = 0; i < s->land_count; ++i) {
            if (s->land_group[i] != s->land_group[id]) continue;
            use->original_land_level[i] = s->land_level[i];
            use->original_land_type[i] = s->land_type[i];
            if (card == 9 || s->land_level[i] < 5) use->highlight_land[i] = 1;
            if (card == 8 && !s->land_type[i] && s->land_level[i] == 4)
                use->max_level_pending = 1;
        }
    }
}

void rich4_card_effect_flash(GameSession *session, uint32_t now)
{
    GamePropertyPresentationState *p = &session->presentation.property;
    p->phase = GAME_PROPERTY_PRESENT_CARD_FLASH;
    p->flash_frame = 0;
    p->flash_adjust = 4;
    p->flash_presented_frame = -1;
    p->tick = now;
    rich4_animation_clock_reset(&session->presentation.animation_clock[RICH4_ANIMATION_PROPERTY_FLASH]);
    (void)rich4_animation_clock_step(
        &session->presentation.animation_clock[RICH4_ANIMATION_PROPERTY_FLASH], now, 30);
    session->presentation.card_use.flash_pending = 0;
}

int rich4_card_effect_flash_update(GameSession *session, uint32_t now)
{
    if (session->presentation.property.phase != GAME_PROPERTY_PRESENT_CARD_FLASH) return 0;
    (void)rich4_property_update_charge_presentation(session, now);
    return session->presentation.property.phase == GAME_PROPERTY_PRESENT_CARD_FLASH;
}
