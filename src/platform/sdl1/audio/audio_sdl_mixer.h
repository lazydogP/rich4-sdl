#ifndef RICH4_SDL_AUDIO_H
#define RICH4_SDL_AUDIO_H

typedef struct Rich4Audio Rich4Audio;
typedef struct Rich4Mkf Rich4Mkf;
typedef struct Rich4AudioEvent Rich4AudioEvent;

int rich4_audio_init(Rich4Audio **out_audio);
void rich4_audio_shutdown(Rich4Audio *audio);
/* 0x45144F: prepared effects begin after the first decoded frame is shown. */
void rich4_audio_begin_visual_frame(Rich4Audio *audio);
void rich4_audio_present_visual_frame(Rich4Audio *audio);
int rich4_audio_load_button_sounds(Rich4Audio *audio, Rich4Mkf *effect_mkf);
int rich4_audio_load_movement_sounds(Rich4Audio *audio, Rich4Mkf *effect_mkf);
int rich4_audio_load_dice_sound(Rich4Audio *audio, Rich4Mkf *effect_mkf);
int rich4_audio_load_jump_sound(Rich4Audio *audio, Rich4Mkf *effect_mkf);
int rich4_audio_load_door_sound(Rich4Audio *audio, Rich4Mkf *effect_mkf);
int rich4_audio_load_property_sounds(Rich4Audio *audio, Rich4Mkf *effect_mkf);
int rich4_audio_load_card_sound(Rich4Audio *audio, Rich4Mkf *effect_mkf);
int rich4_audio_load_god_effect_sounds(Rich4Audio *audio, Rich4Mkf *effect_mkf);
int rich4_audio_load_special_place_sound(Rich4Audio *audio, Rich4Mkf *effect_mkf);
int rich4_audio_load_tool_sounds(Rich4Audio *audio, Rich4Mkf *effect_mkf);
void rich4_audio_set_speaking_archive(Rich4Audio *audio, Rich4Mkf *speaking_mkf);
void rich4_audio_set_effects_enabled(Rich4Audio *audio, int enabled);
int rich4_audio_play_music(Rich4Audio *audio, const char *path);
int rich4_audio_play_music_once(Rich4Audio *audio, const char *path);
int rich4_audio_music_playing(const Rich4Audio *audio);
void rich4_audio_stop_music(Rich4Audio *audio);
void rich4_audio_play_ok(Rich4Audio *audio);
void rich4_audio_play_select(Rich4Audio *audio);
void rich4_audio_play_cancel(Rich4Audio *audio);
void rich4_audio_play_invalid(Rich4Audio *audio);
void rich4_audio_play_target_confirm(Rich4Audio *audio);
void rich4_audio_play_new_game_transition(Rich4Audio *audio);
void rich4_audio_play_bank_key(Rich4Audio *audio);
void rich4_audio_play_bank_slider(Rich4Audio *audio);
void rich4_audio_play_dice(Rich4Audio *audio);
void rich4_audio_play_jump(Rich4Audio *audio);
void rich4_audio_play_door(Rich4Audio *audio);
void rich4_audio_play_property_purchase(Rich4Audio *audio);
void rich4_audio_play_property_upgrade(Rich4Audio *audio);
void rich4_audio_play_property_toll(Rich4Audio *audio);
void rich4_audio_play_property_max_level(Rich4Audio *audio);
void rich4_audio_play_card(Rich4Audio *audio);
void rich4_audio_play_card_animation(Rich4Audio *audio, int resource);
void rich4_audio_play_god_effect(Rich4Audio *audio, int god_type);
void rich4_audio_play_special_place(Rich4Audio *audio);
void rich4_audio_play_auction_effect(Rich4Audio *audio, int resource);
/* Monthly settlement / dividend / drawing resources 27,28,57,58,60,61. */
void rich4_audio_play_finance_effect(Rich4Audio *audio, int resource);
void rich4_audio_stop_finance_effects(Rich4Audio *audio, int resource);
void rich4_audio_play_confinement_visit(Rich4Audio *audio);
void rich4_audio_play_lottery_purchase(Rich4Audio *audio);
void rich4_audio_play_roulette(Rich4Audio *audio);
void rich4_audio_stop_roulette(Rich4Audio *audio);
void rich4_audio_play_magic_hover(Rich4Audio *audio);
void rich4_audio_play_magic_turn(Rich4Audio *audio);
/* News callbacks 0x44913D/0x4492A0/0x44A453/0x44AB2C/0x44AC99. */
void rich4_audio_play_news_consequence(Rich4Audio *audio, int logical_id);
void rich4_audio_play_point_minigame_effect(Rich4Audio *audio, int resource);
void rich4_audio_stop_point_minigame_effect(Rich4Audio *audio, int resource);
void rich4_audio_play_tool(Rich4Audio *audio, int tool);
void rich4_audio_stop_tool_robot(Rich4Audio *audio);
void rich4_audio_play_tool_effect(Rich4Audio *audio, int resource);
void rich4_audio_play_attached_bomb(Rich4Audio *audio);
void rich4_audio_update_attached_bomb(Rich4Audio *audio, int active);
int rich4_audio_play_speech(Rich4Audio *audio, int resource_index);
int rich4_audio_speech_playing(const Rich4Audio *audio);
void rich4_audio_stop_speech(Rich4Audio *audio);
void rich4_audio_update_movement(Rich4Audio *audio, int moving, int traffic_method);
void rich4_audio_consume_event(Rich4Audio *audio, const Rich4AudioEvent *event);

#endif
