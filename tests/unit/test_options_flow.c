#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/options_flow.h"

int main(void)
{
    Rich4OptionsPanelState panel;

    memset(&panel, 0, sizeof(panel));
    panel.active = 1;
    panel.left = 100;
    panel.top = 50;
    panel.saved.music = 2;
    panel.saved.sound_effect = 3;
    panel.draft = panel.saved;
    panel.saved_music_bank = RICH4_MUSIC_BANK_ORIGINAL;
    panel.music_bank = panel.saved_music_bank;
    assert(rich4_options_panel_hit(&panel, 0xc0, 0x46) == 0);
    assert(rich4_options_panel_hit(&panel, 0, 0) == -1);
    assert(rich4_options_panel_apply_click(&panel, 9, 0));
    assert(panel.draft.animation == 1);
    assert(rich4_options_panel_apply_click(&panel, 10, 0));
    assert(panel.draft.music == 0);
    assert(rich4_options_panel_apply_click(&panel, 16, 0));
    assert(panel.music_bank == RICH4_MUSIC_BANK_MULTIVERSE_JOURNEY);
    rich4_options_panel_restore(&panel);
    assert(panel.draft.music == 2 && panel.draft.animation == 0);
    assert(panel.music_bank == RICH4_MUSIC_BANK_ORIGINAL);
    panel.return_app_state = RICH4_SCENE_MENU;
    assert(rich4_options_panel_context_variant(&panel) == 0);
    panel.return_app_state = RICH4_SCENE_GAME;
    assert(rich4_options_panel_context_variant(&panel) == 1);
    {
        Rich4Config config = {0};
        panel.child = 3;
        panel.child_left = panel.child_top = 0;
        panel.child_draft.year = 2000;
        panel.child_draft.month = 2;
        panel.child_draft.day = 1;
        assert(rich4_options_calendar_days(2000, 2) == 29);
        assert(rich4_options_calendar_days(1999, 2) == 28);
        assert(rich4_options_calendar_first(1998, 1) == 4);
        assert(rich4_options_child_hit(&panel, 75, 22) == 0);
        rich4_options_child_apply(&panel, &config, 129);
        rich4_options_child_apply(&panel, &config, 6);
        assert(panel.child == 0 && config.year == 2000 && config.day == 29);
        rich4_options_panel_restore(&panel);
        assert(panel.draft.day == 29); /* Outer cancel keeps date commit. */
        panel.child = 4;
        panel.editing_key = -1;
        rich4_options_child_apply(&panel, &config, 4);
        rich4_options_child_apply(&panel, &config, 207);
        assert(panel.editing_key == -1); /* First eight are read-only. */
        rich4_options_child_apply(&panel, &config, 208);
        assert(!rich4_options_child_key(&panel, 0x4e)); /* Already bound. */
        assert(panel.editing_key == 8);
        assert(rich4_options_child_key(&panel, 0x11));
        assert(rich4_options_child_key(&panel, 0x4e));
        assert(panel.child_draft.hotkeys[8].mod == 0x11);
        rich4_options_child_apply(&panel, &config, 209);
        rich4_options_child_cancel(&panel);
        assert(panel.child == 4 && panel.editing_key == -1);
        assert(panel.child_draft.hotkeys[9].key == 0x4e);
        rich4_options_child_apply(&panel, &config, 6);
        assert(config.hotkeys[8].key == 0x4e && config.hotkeys[8].mod == 0x11);
        rich4_options_panel_restore(&panel);
        assert(panel.draft.hotkeys[8].mod == 0x11);
    }
    puts("app options flow: ok");
    return 0;
}
