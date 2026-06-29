#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "data/special_event_text.h"

int main(void)
{
    static const signed char fortune_portrait[49] = {
         2, 2, 3, 3, 1, 4, 2, 3, 3, 2,
         3, 3, 3, 3, 2, 2, 2, 3, 3, 2,
         1, 4, 4, 2, 3, 4, 3, 1, 4, 4,
         2, 4, 2, 3, 3, 3, 3, 3, 3, 3,
         3, 3, 3, 3, 3, 3, 3, 3, 3
    };
    char text[256];
    int i;
    for (i = 0; i < 36; i++) {
        const char *raw = rich4_news_text_big5(i);
        assert(raw != NULL && raw[0] == '#');
        assert(rich4_news_title_big5(i)[0] != '\0');
    }
    for (i = 0; i < 49; i++) {
        const char *raw = rich4_fortune_text_big5(i);
        assert(raw != NULL && raw[0] == '#');
        assert(rich4_fortune_portrait_chunk(i) == fortune_portrait[i]);
    }
    assert(strcmp(rich4_news_text_big5(-1), "") == 0);
    assert(strcmp(rich4_fortune_text_big5(49), "") == 0);
    assert(rich4_fortune_portrait_chunk(-1) == -1);
    assert(rich4_fortune_portrait_chunk(49) == -1);
    assert(rich4_special_event_format_big5(
        text, sizeof(text), "#0001%s:%d%%", "P", 42));
    assert(strcmp(text, "P:42%") == 0);
    assert(rich4_special_event_format_names_big5(
        text,sizeof(text),"#0178%s/%s", "Company", "Operator",0));
    assert(strcmp(text,"Company/Operator") == 0);
    assert(strcmp(rich4_news_title_big5(6),rich4_news_title_big5(13)) == 0);
    assert(strcmp(rich4_news_title_big5(14),rich4_news_title_big5(15)) == 0);
    assert(strcmp(rich4_news_title_big5(16),rich4_news_title_big5(17)) == 0);
    assert(strcmp(rich4_news_title_big5(18),rich4_news_title_big5(21)) == 0);
    assert(strcmp(rich4_news_title_big5(22),rich4_news_title_big5(35)) == 0);
    assert(strcmp(rich4_news_title_big5(5),rich4_news_title_big5(6)) != 0);
    assert(!rich4_special_event_format_big5(
        text, 4, "#0001%s:%d", "long", 42));
    /* 0x465888/0x4658AE/0x4658E7: income, fine, days notices. */
    assert(rich4_fortune_god_notice_format_big5(20,2)!=NULL);
    assert(rich4_fortune_god_notice_format_big5(14,2)!=NULL);
    assert(rich4_fortune_god_notice_format_big5(6,1)!=NULL);
    assert(strcmp(rich4_fortune_god_notice_format_big5(20,2),
        "\x25\x73\xab\x4f\xa6\xf6\x0a\x0a\xbc\xfa\xaa\xf7\xa5\x5b\xad\xbf\xa1\x49")==0);
    assert(rich4_fortune_god_notice_format_big5(3,2)==NULL);
    assert(rich4_fortune_god_notice_format_big5(8,2)==NULL);
    assert(rich4_fortune_god_notice_format_big5(32,2)==NULL);
    assert(rich4_fortune_god_notice_format_big5(5,1)==NULL);
    assert(rich4_fortune_god_notice_format_big5(6,0)==NULL);
    assert(strcmp(rich4_special_event_god_name_big5(8),"\xa4\x6a\xb0\x49\xaf\xab")==0);
    puts("special event text catalog: ok");
    return 0;
}
