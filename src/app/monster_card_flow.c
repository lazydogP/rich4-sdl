#include "app/monster_card_flow.h"

#include "app/game_speech_flow.h"

int rich4_monster_card_flow_update(GameSession *session, int frame_count,
                                   uint32_t now)
{
    int speech_player;

    if (session == NULL || !session->presentation.monster_card.active)
        return 0;
    if (session->presentation.monster_card.frame < frame_count) {
        if (rich4_animation_clock_step(
                &session->presentation.animation_clock[RICH4_ANIMATION_MONSTER],
                now, 80)) session->presentation.monster_card.frame++;
        session->presentation.monster_card.until = now + 500;
        return 1;
    }
    if ((int32_t)(now - session->presentation.monster_card.until) < 0)
        return 1;
    speech_player = session->presentation.monster_card.speech_player;
    session->presentation.monster_card.active = 0;
    session->presentation.monster_card.sound_resource = 0;
    session->presentation.monster_card.speech_player = -1;
    if (speech_player >= 0)
        game_start_monster_victim_speech(session, speech_player, now);
    return 0;
}
