#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_HELP_DATA_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_HELP_DATA_H

enum {
    HELP_LEFT = 20,
    HELP_TOP = 60,
    HELP_CATEGORY_COUNT = 8,
    HELP_VISIBLE_ITEMS = 8,
    HELP_BODY_LINES = 14,
    HELP_ITEM_NAME_COUNT = 98
};

extern const int help_category_item_offset[HELP_CATEGORY_COUNT];
extern const int help_category_item_count[HELP_CATEGORY_COUNT];
extern const char *const help_item_names[];
extern const char *const help_category_names[HELP_CATEGORY_COUNT];
int help_item_label_index(int category, int item);

#endif
