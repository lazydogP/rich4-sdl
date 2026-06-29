#ifndef RICH4_DATA_CHARACTER_SPEECH_H
#define RICH4_DATA_CHARACTER_SPEECH_H

#include <stdint.h>

typedef struct GamePlayerState GamePlayerState;
typedef struct GameState GameState;

typedef enum Rich4PropertySpeechKind {
    RICH4_PROPERTY_SPEECH_RECEIVER_HIGH_1 = 0,
    RICH4_PROPERTY_SPEECH_RECEIVER_HIGH_2,
    RICH4_PROPERTY_SPEECH_RECEIVER_LOW,
    RICH4_PROPERTY_SPEECH_PAYER_HIGH_1,
    RICH4_PROPERTY_SPEECH_PAYER_HIGH_2,
    RICH4_PROPERTY_SPEECH_PAYER_LOW,
    RICH4_PROPERTY_SPEECH_PURCHASE_THREE,
    RICH4_PROPERTY_SPEECH_PURCHASE_RANDOM,
    RICH4_PROPERTY_SPEECH_PAYER_RELATION,
    RICH4_PROPERTY_SPEECH_MAX_LEVEL,
    RICH4_PROPERTY_SPEECH_COUNT
} Rich4PropertySpeechKind;

typedef struct Rich4CharacterSpeech {
    int speaking_resource;
    const char *big5;
} Rich4CharacterSpeech;

enum { RICH4_CHARACTER_SPEECH_TEXT_CAPACITY = 512 };

/* Pointer-free copy used by session persistence for presentation speech. */
typedef struct Rich4CharacterSpeechSnapshot {
    int valid;
    int speaking_resource;
    char big5[RICH4_CHARACTER_SPEECH_TEXT_CAPACITY];
} Rich4CharacterSpeechSnapshot;

int rich4_character_speech_snapshot_capture(
    Rich4CharacterSpeechSnapshot *snapshot,
    const Rich4CharacterSpeech *speech);
int rich4_character_speech_snapshot_capture_text(
    Rich4CharacterSpeechSnapshot *snapshot, const char *big5);
int rich4_character_speech_snapshot_is_valid(
    const Rich4CharacterSpeechSnapshot *snapshot);
int rich4_character_speech_snapshot_restore(
    const Rich4CharacterSpeechSnapshot *snapshot,
    Rich4CharacterSpeech *storage);
void rich4_character_speech_snapshot_clear(
    Rich4CharacterSpeechSnapshot *snapshot);

typedef struct Rich4PlayerSayText {
    const char *display_text;
    int speaking_resource;
    int icon_chunk;
} Rich4PlayerSayText;

typedef enum Rich4HospitalSpeechKind {
    RICH4_HOSPITAL_SPEECH_GREETING = 0,
    RICH4_HOSPITAL_SPEECH_INSUFFICIENT,
    RICH4_HOSPITAL_SPEECH_RELEASE_APPROVED,
    RICH4_HOSPITAL_SPEECH_GOODBYE,
    RICH4_HOSPITAL_SPEECH_COUNT
} Rich4HospitalSpeechKind;

const Rich4CharacterSpeech *rich4_character_god_arrival_speech(int character);
const Rich4CharacterSpeech *rich4_character_god_departure_speech(int character);
const Rich4CharacterSpeech *rich4_character_property_speech(
    int character, Rich4PropertySpeechKind kind);
/* 0x4808AA: winner speech selected by character at the terminal victory
 * transition (resources 1074..1085). */
const Rich4CharacterSpeech *rich4_character_victory_speech(int character);
/* 0x4808AE + character*0x6C: type-2 speech emitted after the final
 * bankruptcy auction while more than one active player remains. */
const Rich4CharacterSpeech *rich4_character_bankruptcy_speech(int character);
const Rich4CharacterSpeech *rich4_character_monster_victim_speech(
    int character);
/* _tool_strings[character][tool] at 0x480D5A.  tool is zero-based here. */
const Rich4CharacterSpeech *rich4_character_tool_use_speech(
    int character, int tool);
/* _tool_strings[character][14..16] at 0x480D92/0x480D96/0x480D9A:
 * roadblock, mine, attached-bomb pickup; tool is 1..3. */
const Rich4CharacterSpeech *rich4_character_tool_trigger_speech(
    int character, int tool);
/* 0x48084A: per-character action 10 high reward has two variants;
 * action 11 uses the following low-reward entry. */
const Rich4CharacterSpeech *rich4_character_reward_speech(
    int character, int high_reward, int variant);
/* Original character-record entries +0x0C/+0x10 (0x480856) and +0x14
 * (0x48085E), used by news/fortune reaction callbacks. */
const Rich4CharacterSpeech *rich4_character_special_event_speech(
    int character, int fixed_reaction, int variant);
/* sub_44F567: speech used when a special-event payment is blocked. */
const Rich4CharacterSpeech *rich4_character_payment_blocked_speech(
    int character, int amount, int price_index, int variant);
/* sub_41D559 always uses character-record entry +0x34 for the player
 * standing on an opponent's property when collection is suppressed. */
const Rich4CharacterSpeech *rich4_character_property_collection_blocked_speech(
    int character);
/* Original character-record entries +0x4C/+0x50
 * (0x480896/0x48089A), used after prison/hospital entry. */
const Rich4CharacterSpeech *rich4_character_confinement_speech(
    int character, int hospital);
/* 0x44F230: shop-owner gift speech selected by the gifted item's price. */
const Rich4CharacterSpeech *rich4_character_shop_gift_speech(
    int character, int shop_price, int variant);
const Rich4CharacterSpeech *rich4_lottery_counter_speech(int index);
/* 0x43DA27 uses these Panel[65] bubble lines.  The four special-player
 * entries are indexed in hospital slot order (thief, robber, hooligan, spy). */
const Rich4CharacterSpeech *rich4_hospital_nurse_speech(
    Rich4HospitalSpeechKind kind);
const Rich4CharacterSpeech *rich4_hospital_rescued_special_speech(
    int special_player);
/* 0x43D1FD indexes the same 0x475BF4 four-line table while rescuing a
 * special actor from Panel[63]. */
const Rich4CharacterSpeech *rich4_prison_rescued_special_speech(
    int special_player);
/* sub_43CAAB uses the #0002 voice marker and its following visible line. */
const Rich4CharacterSpeech *rich4_prison_insufficient_speech(void);
/* 0x481282/0x481286/0x481372 + character*360: redirect/free/victim. */
const Rich4CharacterSpeech *rich4_character_insurance_card_speech(int character, int kind);
/* 0x444BB2: automatic absolution, distinct from the free-payment card. */
const Rich4CharacterSpeech *rich4_character_absolution_speech(int character);
#endif
