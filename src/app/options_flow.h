#ifndef RICH4_APP_OPTIONS_FLOW_H
#define RICH4_APP_OPTIONS_FLOW_H

#include "app/config_types.h"
#include "app/scene.h"
#include "data/music.h"

/* App-owned options draft and hit/edit semantics. It contains no SDL
 * surface or file handle; platform owns preview playback and persistence. */
typedef struct Rich4OptionsPanelState {
    int active;
    int left;
    int top;
    int width;
    int height;
    int hover;
    /* In-game action controls 3/4/5 are committed on button-up, like the
     * original WM_LBUTTONDOWN/WM_LBUTTONUP pair. */
    int pressed_control;
    /* Non-zero while the in-game action confirmation resource is shown. */
    int pending_action;
    /* 0=none, 1=accept, 2=decline; kept numeric to avoid SDL coupling. */
    int confirm_hover;
    Rich4SceneId return_app_state;
    int music_track;
    Rich4MusicBank music_bank;
    Rich4MusicBank saved_music_bank;
    int music_preview_changed;
    /* Title-menu nested modals, 0=closed, 3=date, 4=hotkeys, 5=help.
     * Transient UI state; not added to the original save format. */
    int child;
    int child_left, child_top;
    int child_pressed;
    int confirm_key_action;
    int editing_key;
    Rich4Key editing_saved;
    Rich4Config child_draft;
    Rich4Config today;
    Rich4Config saved;
    Rich4Config draft;
} Rich4OptionsPanelState;

int rich4_options_calendar_days(int year, int month);
int rich4_options_calendar_first(int year, int month);
int rich4_options_child_hit(const Rich4OptionsPanelState *panel, int x, int y);
void rich4_options_child_apply(Rich4OptionsPanelState *panel,
                              Rich4Config *config, int control);
int rich4_options_child_key(Rich4OptionsPanelState *panel, int vk);
void rich4_options_child_cancel(Rich4OptionsPanelState *panel);
void rich4_options_panel_reset(Rich4OptionsPanelState *panel);
int rich4_options_panel_hit(const Rich4OptionsPanelState *panel,
                            int x, int y);
void rich4_options_panel_set_hover(Rich4OptionsPanelState *panel, int hover);
void rich4_options_panel_set_pressed(Rich4OptionsPanelState *panel,
                                     int control);
int rich4_options_panel_pressed(const Rich4OptionsPanelState *panel);
int rich4_options_panel_begin_action(Rich4OptionsPanelState *panel,
                                     int control);
void rich4_options_panel_cancel_action(Rich4OptionsPanelState *panel);
int rich4_options_panel_has_pending_action(
    const Rich4OptionsPanelState *panel);
int rich4_options_panel_pending_action(
    const Rich4OptionsPanelState *panel);
void rich4_options_panel_set_confirm_hover(
    Rich4OptionsPanelState *panel, int hover);
int rich4_options_panel_confirm_hover(
    const Rich4OptionsPanelState *panel);
/* The original options callback receives 0 from the title menu and 1 from
 * the in-game toolbar.  It selects the matching text and background group. */
int rich4_options_panel_context_variant(const Rich4OptionsPanelState *panel);
int rich4_options_panel_apply_click(Rich4OptionsPanelState *panel,
                                    int control, int x);
void rich4_options_panel_restore(Rich4OptionsPanelState *panel);

#endif
