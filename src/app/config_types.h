#ifndef RICH4_APP_CONFIG_TYPES_H
#define RICH4_APP_CONFIG_TYPES_H

/* Persisted application configuration. The SDL adapter owns file I/O; the
 * values and hotkey shape are part of app configuration, not SDL state. */
typedef struct Rich4Key {
    unsigned char key;
    unsigned char mod;
} Rich4Key;

typedef struct Rich4Config {
    unsigned char game_speed;
    unsigned char animation;
    unsigned char music;
    unsigned char sound_effect;
    unsigned char auto_save;
    unsigned char view;
    unsigned char dummy1[2];
    unsigned char day;
    unsigned char month;
    unsigned short year;
    /* RICH4.CFG +12 / 0x497164: 0 daily calendar, 1 monthly calendar. */
    unsigned char calendar_view;
    unsigned char dummy2[3];
    Rich4Key hotkeys[28];
} Rich4Config;

#endif
