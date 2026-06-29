#ifndef RICH4_DATA_SPECIAL_EVENT_TEXT_H
#define RICH4_DATA_SPECIAL_EVENT_TEXT_H

#include <stddef.h>

const char *rich4_news_title_big5(int logical_id);
const char *rich4_news_text_big5(int logical_id);
const char *rich4_fortune_text_big5(int physical_id);
int rich4_fortune_portrait_chunk(int physical_id);
/* 0x44B896 strings; NULL when this callback does not show a notice. */
const char *rich4_fortune_god_notice_format_big5(int physical_id, int defense);
const char *rich4_special_event_god_name_big5(int god_type);

/* Removes an optional #dddd speaking marker and expands the original %s/%d
 * placeholders without passing untrusted templates through varargs. */
int rich4_special_event_format_big5(char *out, size_t out_size,
                                    const char *marked_template,
                                    const char *name_big5, int number);
int rich4_special_event_format_names_big5(char *out, size_t out_size,
    const char *marked_template, const char *first_name,
    const char *second_name, int number);

#endif
