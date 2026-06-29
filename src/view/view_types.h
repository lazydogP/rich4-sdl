#ifndef RICH4_VIEW_TYPES_H
#define RICH4_VIEW_TYPES_H

typedef struct Rich4Big5Utf8Map {
    const char *big5;
    const char *utf8;
} Rich4Big5Utf8Map;

typedef struct GameVisibleMapObject {
    int asset_index;
    int sprite;
    int object_id;
    int owner;
    int variant;
    int x;
    int y;
    int sort_key;
} GameVisibleMapObject;

#endif
