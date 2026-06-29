#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "data/character_speech.h"
#include "core/character_speech_rules.h"

int main(void)
{
    GameState state;
    const Rich4CharacterSpeech *speech;
    const char *last_text = NULL;
    Rich4PlayerSayText decoded;
    static const char repeated[] = "same-address";
    static const char same_bytes[] = "same-address";
    static const char prefixed[] = "#0123@17";
    memset(&state, 0, sizeof(state));
    state.player_count = 3;
    state.price_index = 1;
    state.players[0].play_state = 1;
    state.players[1].play_state = 1;
    state.players[2].play_state = 1;
    state.players[0].relation_balance[1] = 20;
    state.players[0].relation_balance[2] = 10;
    state.land_count = 4;
    state.land_group[0] = state.land_group[1] = state.land_group[2] = 7;
    state.land_group[3] = 8;
    state.land_owner[0] = state.land_owner[1] = state.land_owner[2] = 1;
    state.land_owner[3] = 1;

    speech = rich4_character_property_speech(
        0, RICH4_PROPERTY_SPEECH_PURCHASE_THREE);
    assert(speech != NULL && speech->speaking_resource == 1066);
    speech = rich4_character_victory_speech(0);
    assert(speech != NULL && speech->speaking_resource == 1074);
    rich4_player_say_decode(speech->big5, speech->speaking_resource, &decoded);
    assert(decoded.speaking_resource == 1074 &&
           decoded.display_text == speech->big5 + 5 &&
           decoded.display_text[0] != '\0');
    speech = rich4_character_victory_speech(11);
    assert(speech != NULL && speech->speaking_resource == 1085);
    assert(rich4_character_victory_speech(-1) == NULL &&
           rich4_character_victory_speech(12) == NULL);
    speech = rich4_character_property_speech(
        11, RICH4_PROPERTY_SPEECH_PAYER_RELATION);
    assert(speech != NULL && speech->speaking_resource == 1365 &&
           strcmp(speech->big5, "@08") == 0);
    speech = rich4_character_property_speech(
        0, RICH4_PROPERTY_SPEECH_MAX_LEVEL);
    assert(speech != NULL && speech->speaking_resource == 1065);
    speech = rich4_character_property_speech(
        11, RICH4_PROPERTY_SPEECH_MAX_LEVEL);
    assert(speech != NULL && speech->speaking_resource == 1362 &&
           strcmp(speech->big5, "@04") == 0);
    speech = rich4_character_monster_victim_speech(0);
    assert(speech != NULL);
    rich4_player_say_decode(speech->big5, speech->speaking_resource, &decoded);
    assert(decoded.speaking_resource == 467 &&
           decoded.display_text == speech->big5 + 5);
    speech = rich4_character_monster_victim_speech(11);
    assert(speech != NULL);
    rich4_player_say_decode(speech->big5, speech->speaking_resource, &decoded);
    assert(decoded.speaking_resource == 1039 &&
           decoded.icon_chunk == 7);
    assert(rich4_character_monster_victim_speech(-1) == NULL &&
           rich4_character_monster_victim_speech(12) == NULL);
    {
        static const int tool_voice_offset[13] = {
            0, 1, 2, 3, -2, -1, 4, 5, 10, 6, 7, 8, 9
        };
        int character;
        int tool;
        for (character = 0; character < 12; character++) {
            for (tool = 0; tool < 13; tool++) {
                speech = rich4_character_tool_use_speech(character, tool);
                assert(speech != NULL && speech->big5 != NULL);
                rich4_player_say_decode(
                    speech->big5, speech->speaking_resource, &decoded);
                assert(decoded.speaking_resource ==
                       (character == 11
                            ? 410 + tool
                            : 236 + character * 16 +
                                  tool_voice_offset[tool]));
            }
        }
        assert(rich4_character_tool_use_speech(-1, 0) == NULL &&
               rich4_character_tool_use_speech(12, 0) == NULL &&
               rich4_character_tool_use_speech(0, -1) == NULL &&
               rich4_character_tool_use_speech(0, 13) == NULL);
    }
    speech = rich4_character_tool_trigger_speech(0, 1);
    assert(speech != NULL);
    rich4_player_say_decode(
        speech->big5, speech->speaking_resource, &decoded);
    assert(decoded.speaking_resource == 247);
    speech = rich4_character_tool_trigger_speech(11, 2);
    assert(speech != NULL);
    rich4_player_say_decode(
        speech->big5, speech->speaking_resource, &decoded);
    assert(decoded.speaking_resource == 424 && decoded.icon_chunk == 6);
    assert(rich4_character_tool_trigger_speech(0, 0) == NULL &&
           rich4_character_tool_trigger_speech(0, 4) == NULL &&
           rich4_character_tool_trigger_speech(-1, 1) == NULL);
    speech = rich4_character_tool_trigger_speech(0, 3);
    assert(speech != NULL);
    rich4_player_say_decode(speech->big5, speech->speaking_resource, &decoded);
    assert(decoded.speaking_resource == 249);
    speech = rich4_character_reward_speech(0, 1, 0);
    assert(speech != NULL && speech->speaking_resource == 1050);
    speech = rich4_character_reward_speech(0, 1, 1);
    assert(speech != NULL && speech->speaking_resource == 1051);
    speech = rich4_character_reward_speech(0, 0, 1);
    assert(speech != NULL && speech->speaking_resource == 1052);
    speech = rich4_character_reward_speech(11, 1, 0);
    assert(speech != NULL && speech->speaking_resource == 1347);
    speech = rich4_character_reward_speech(11, 1, 1);
    assert(speech != NULL && speech->speaking_resource == 1348);
    speech = rich4_character_reward_speech(11, 0, 0);
    assert(speech != NULL && speech->speaking_resource == 1349);
    assert(rich4_character_reward_speech(-1, 1, 0) == NULL &&
           rich4_character_reward_speech(12, 0, 0) == NULL);
    speech = rich4_character_special_event_speech(0, 0, 0);
    assert(speech != NULL && speech->speaking_resource == 1053);
    speech = rich4_character_special_event_speech(0, 0, 1);
    assert(speech != NULL && speech->speaking_resource == 1054);
    speech = rich4_character_special_event_speech(0, 1, 0);
    assert(speech != NULL && speech->speaking_resource == 1055);
    speech = rich4_character_special_event_speech(11, 0, 0);
    assert(speech != NULL && speech->speaking_resource == 1350);
    speech = rich4_character_special_event_speech(11, 0, 1);
    assert(speech != NULL && speech->speaking_resource == 1351);
    speech = rich4_character_special_event_speech(11, 1, 0);
    assert(speech != NULL && speech->speaking_resource == 1352);
    assert(rich4_character_special_event_speech(-1, 0, 0) == NULL &&
           rich4_character_special_event_speech(12, 1, 0) == NULL);
    speech = rich4_character_payment_blocked_speech(0, 9000, 1, 1);
    assert(speech != NULL && speech->speaking_resource == 1062);
    speech = rich4_character_payment_blocked_speech(0, 5000, 1, 1);
    assert(speech != NULL && speech->speaking_resource == 1063);
    speech = rich4_character_payment_blocked_speech(0, 4999, 1, 0);
    assert(speech != NULL && speech->speaking_resource == 1064);
    speech = rich4_character_payment_blocked_speech(11, 18000, 2, 1);
    assert(speech != NULL && speech->speaking_resource == 1359);
    speech = rich4_character_property_collection_blocked_speech(0);
    assert(speech != NULL && speech->speaking_resource == 1063);
    speech = rich4_character_property_collection_blocked_speech(11);
    assert(speech != NULL && speech->speaking_resource == 1360);
    assert(rich4_character_payment_blocked_speech(0, 0, 1, 0) == NULL &&
           rich4_character_payment_blocked_speech(-1, 9000, 1, 0) == NULL &&
           rich4_character_payment_blocked_speech(12, 9000, 1, 0) == NULL &&
           rich4_character_property_collection_blocked_speech(-1) == NULL &&
           rich4_character_property_collection_blocked_speech(12) == NULL);
    /* A voice prefix alone produced an empty bubble for all 11 speaking
     * characters. Decode both confinement entries and require visible text. */
    {
        int character, hospital;
        for (character = 0; character < 11; character++) {
            for (hospital = 0; hospital < 2; hospital++) {
                speech = rich4_character_confinement_speech(character, hospital);
                assert(speech != NULL);
                rich4_player_say_decode(
                    speech->big5, speech->speaking_resource, &decoded);
                assert(decoded.speaking_resource == 1069 + character * 27 + hospital);
                assert(decoded.display_text == speech->big5 + 5 &&
                       decoded.display_text[0] != '\0' && decoded.icon_chunk == -1);
            }
        }
    }
    /* V3.11 table pointer 0x480896. */
    speech = rich4_character_confinement_speech(0, 0);
    assert(strcmp(speech->big5, "\x23\x31\x30\x36\x39\xa9\xf1\xa7\xda\xa5\x58\xa5\x68\xa1\x49") == 0);
    /* V3.11 table pointer 0x48089A. */
    speech = rich4_character_confinement_speech(0, 1);
    assert(strcmp(speech->big5, "\x23\x31\x30\x37\x30\xa7\xda\xa4\xa3\xad\x6e\xa5\xb4\xb0\x77\xa1\x49\xa1\x49") == 0);
    /* V3.11 table pointer 0x480972. */
    speech = rich4_character_confinement_speech(2, 1);
    assert(strcmp(speech->big5, "\x23\x31\x31\x32\x34\xa8\x6b\xa8\xe0\xa6\xb3\xb2\x5c\x0a\xa4\xa3\xbb\xb4\xbc\x75\xa1\xe3") == 0);
    /* V3.11 table pointer 0x480AB6. */
    speech = rich4_character_confinement_speech(5, 1);
    assert(strcmp(speech->big5, "\x23\x31\x32\x30\x35\xa7\xda\xad\x6e\xa6\xed\x0a\xc0\x59\xb5\xa5\xaf\x66\xa9\xd0\xa1\x49\xa1\x49\xa1\x49") == 0);
    /* V3.11 table pointer 0x480B1E. */
    speech = rich4_character_confinement_speech(6, 0);
    assert(strcmp(speech->big5, "\x23\x31\x32\x33\x31\xa9\xf1\xa7\xda\xa5\x58\xa5\x68\xa1\x49\x0a\xa7\xda\xac\x4f\xb5\x4c\xb6\x64\xaa\xba\xa1\x49") == 0);
    /* V3.11 table pointer 0x480B22. */
    speech = rich4_character_confinement_speech(6, 1);
    assert(strcmp(speech->big5, "\x23\x31\x32\x33\x32\xb6\xd9\xa1\x41\xba\x7d\xab\x47\xaa\xba\xc5\x40\xa4\x68\x0a\xa9\x66\xa9\x66\xa6\x62\xad\xfe\xa8\xe0\xa1\x48") == 0);
    speech = rich4_character_confinement_speech(0, 0);
    assert(speech != NULL && speech->speaking_resource == 1069);
    speech = rich4_character_confinement_speech(0, 1);
    assert(speech != NULL && speech->speaking_resource == 1070);
    speech = rich4_character_confinement_speech(11, 0);
    assert(speech != NULL && speech->speaking_resource == 1366 &&
           strcmp(speech->big5, "#1366@10") == 0);
    speech = rich4_character_confinement_speech(11, 1);
    assert(speech != NULL && speech->speaking_resource == 1367 &&
           strcmp(speech->big5, "#1367@17") == 0);
    assert(rich4_character_confinement_speech(-1, 0) == NULL &&
           rich4_character_confinement_speech(12, 1) == NULL);
    speech = rich4_hospital_nurse_speech(RICH4_HOSPITAL_SPEECH_GREETING);
    assert(speech != NULL && speech->speaking_resource == 127 &&
           strcmp(speech->big5, "#0127\xb1\x7a\xa6\x6e\xa1\x49\xbd\xd0\xb0\xdd\xb1\x7a\xad\x6e\xb4\xc0\xbd\xd6\x0a"
                                "\xbf\xec\xb2\x7a\xa5\x58\xb0\x7c\xa4\xe2\xc4\xf2\xa1\x48") == 0);
    speech = rich4_hospital_nurse_speech(
        RICH4_HOSPITAL_SPEECH_INSUFFICIENT);
    assert(speech != NULL && speech->speaking_resource == 2);
    speech = rich4_hospital_nurse_speech(
        RICH4_HOSPITAL_SPEECH_RELEASE_APPROVED);
    assert(speech != NULL && speech->speaking_resource == 129);
    speech = rich4_hospital_nurse_speech(RICH4_HOSPITAL_SPEECH_GOODBYE);
    assert(speech != NULL && speech->speaking_resource == 130);
    speech = rich4_hospital_rescued_special_speech(0);
    assert(speech != NULL && speech->speaking_resource == 124);
    speech = rich4_hospital_rescued_special_speech(1);
    assert(speech != NULL && speech->speaking_resource == 125);
    speech = rich4_hospital_rescued_special_speech(2);
    assert(speech != NULL && speech->speaking_resource == 123);
    speech = rich4_hospital_rescued_special_speech(3);
    assert(speech != NULL && speech->speaking_resource == 126);
    speech = rich4_prison_rescued_special_speech(0);
    assert(speech != NULL && speech->speaking_resource == 124);
    speech = rich4_prison_rescued_special_speech(3);
    assert(speech != NULL && speech->speaking_resource == 126);
    speech = rich4_prison_insufficient_speech();
    assert(speech != NULL && speech->speaking_resource == 2 &&
           strcmp(speech->big5,
                  "#0002\xa9\xea\xba\x70\xa1\x49\x0a"
                  "\xa7\x41\xaa\xba\xc2\x49\xbc\xc6\xa4\xa3\xa8\xac\xa1\x49") == 0);
    rich4_player_say_decode(
        speech->big5, speech->speaking_resource, &decoded);
    assert(decoded.speaking_resource == 2 &&
           decoded.display_text == speech->big5 + 5 &&
           decoded.display_text[0] != '\0');
    assert(rich4_hospital_nurse_speech(RICH4_HOSPITAL_SPEECH_COUNT) == NULL &&
           rich4_hospital_rescued_special_speech(-1) == NULL &&
           rich4_hospital_rescued_special_speech(4) == NULL &&
           rich4_prison_rescued_special_speech(-1) == NULL &&
           rich4_prison_rescued_special_speech(4) == NULL);
    speech = rich4_character_shop_gift_speech(0, 50, 1);
    assert(speech != NULL && speech->speaking_resource == 1052);
    speech = rich4_character_shop_gift_speech(0, 51, 0);
    assert(speech != NULL && speech->speaking_resource == 1050);
    speech = rich4_character_shop_gift_speech(0, 100, 1);
    assert(speech != NULL && speech->speaking_resource == 1051);
    speech = rich4_character_shop_gift_speech(0, 101, 1);
    assert(speech != NULL && speech->speaking_resource == 1050);
    assert(rich4_character_shop_gift_speech(0, 0, 0) == NULL &&
           rich4_character_shop_gift_speech(-1, 50, 0) == NULL &&
           rich4_character_shop_gift_speech(12, 50, 0) == NULL);
    {
        int lottery;
        for (lottery = 0; lottery < 6; lottery++) {
            speech = rich4_lottery_counter_speech(lottery);
            assert(speech != NULL &&
                   speech->speaking_resource == lottery + 11);
            rich4_player_say_decode(
                speech->big5, speech->speaking_resource, &decoded);
            assert(decoded.speaking_resource == lottery + 11 &&
                   decoded.display_text == speech->big5 + 5);
        }
        assert(rich4_lottery_counter_speech(-1) == NULL &&
               rich4_lottery_counter_speech(6) == NULL);
    }
    assert(rich4_property_same_group_owned_count(&state, 1, 0) == 3);
    assert(rich4_property_toll_flash_count(&state, 0) == 3);
    {
        GameState flash = state;
        flash.land_count = 6;
        flash.players[1].linked_player = 2;
        flash.land_owner[1] = 2;
        flash.land_owner[4] = 0;
        flash.land_group[4] = 7;
        flash.land_owner[5] = 2;
        flash.land_group[5] = 8;
        assert(rich4_property_toll_flash_count(&flash, 0) == 3);
        assert(rich4_property_toll_flash_contains(&flash, 0, 1));
        assert(!rich4_property_toll_flash_contains(&flash, 0, 3));
        assert(!rich4_property_toll_flash_contains(&flash, 0, 4));
        flash.land_type[0] = flash.land_type[1] = flash.land_type[3] = 1;
        flash.land_type[4] = flash.land_type[5] = 2;
        assert(rich4_property_toll_flash_count(&flash, 0) == 4);
        assert(rich4_property_toll_flash_contains(&flash, 0, 5));
        assert(!rich4_property_toll_flash_contains(&flash, 0, 2));
        assert(!rich4_property_toll_flash_contains(&flash, 0, 4));
        flash.players[1].linked_player = -1;
        assert(rich4_property_toll_flash_count(&flash, 0) == 2);
    }
    assert(rich4_property_relation_target(&state, 0) == 1);
    assert(rich4_property_payer_speech(&state, 0, 1, 5000, 1, 0) ==
           RICH4_PROPERTY_SPEECH_PAYER_RELATION);
    assert(rich4_property_payer_speech(&state, 0, 2, 1, 1, 0) ==
           RICH4_PROPERTY_SPEECH_PAYER_LOW);
    assert(rich4_property_payer_speech(&state, 0, 2, 0, 1, 0) ==
           RICH4_PROPERTY_SPEECH_COUNT);
    assert(rich4_property_receiver_speech(&state, 1999, 1) ==
           RICH4_PROPERTY_SPEECH_COUNT);
    assert(rich4_property_receiver_speech(&state, 9000, 1) ==
           RICH4_PROPERTY_SPEECH_RECEIVER_HIGH_1);
    assert(rich4_player_speech_allowed(&state.players[0]));
    assert(rich4_player_say_try_begin(&last_text, repeated,
                                      &state.players[0]));
    assert(last_text == repeated);
    assert(!rich4_player_say_try_begin(&last_text, repeated,
                                       &state.players[0]));
    /* Equal bytes at a different address are a different original call. */
    assert(rich4_player_say_try_begin(&last_text, same_bytes,
                                      &state.players[0]));
    state.players[0].days_sleep_walking = 1;
    assert(!rich4_player_speech_allowed(&state.players[0]));
    assert(!rich4_player_say_try_begin(&last_text, repeated,
                                       &state.players[0]));
    assert(last_text == repeated);
    state.players[0].days_sleep_walking = 0;
    assert(!rich4_player_say_try_begin(&last_text, repeated,
                                       &state.players[0]));
    assert(rich4_player_say_try_begin(&last_text, NULL,
                                      &state.players[0]));
    assert(last_text == NULL);
    assert(!rich4_player_say_try_begin(&last_text, NULL,
                                       &state.players[0]));

    rich4_player_say_decode(prefixed, 77, &decoded);
    assert(decoded.speaking_resource == 123);
    assert(decoded.display_text == prefixed + 5);
    assert(decoded.icon_chunk == 16);
    assert(strcmp(prefixed, "#0123@17") == 0);
    rich4_player_say_decode("plain", 77, &decoded);
    assert(decoded.speaking_resource == 77 &&
           strcmp(decoded.display_text, "plain") == 0 &&
           decoded.icon_chunk == -1);
    assert(rich4_player_say_should_wait(0, 0, 0));
    assert(rich4_player_say_should_wait(999, 0, 0));
    assert(!rich4_player_say_should_wait(1000, 0, 0));
    assert(rich4_player_say_should_wait(1000, 1, 0));
    assert(rich4_player_say_should_wait(5000, 1, 0));
    assert(!rich4_player_say_should_wait(0, 1, 1));
    {
        int character;
        for (character=0;character<12;character++) {
            const Rich4CharacterSpeech *immune=rich4_character_absolution_speech(character);
            assert(immune && immune->speaking_resource==446+52*character);
            assert(immune!=rich4_character_insurance_card_speech(character,1));
        }
        assert(!rich4_character_absolution_speech(-1));
        assert(!rich4_character_absolution_speech(12));
    }
    puts("character property speech table and thresholds: ok");
    return 0;
}
