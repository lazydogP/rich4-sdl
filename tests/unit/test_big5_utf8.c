#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "platform/sdl1/text/big5_utf8.h"

int main(void)
{
    char out[64];

    /* 取消, Big5 A8FA AEF8. Original UI literal in runtime_text.c. */
    assert(rich4_big5_to_utf8("\xa8\xfa\xae\xf8", out, sizeof(out)));
    assert(strcmp(out, "\xe5\x8f\x96\xe6\xb6\x88") == 0);
    assert(rich4_cp950_to_ucs2(0xa8fa) == 0x53d6);
    assert(rich4_cp950_to_ucs2(0xaef8) == 0x6d88);

    /* 確定, Big5 BD54 A977. Options OK button literal. */
    assert(rich4_big5_to_utf8("\xbd\x54\xa9\x77", out, sizeof(out)));
    assert(strcmp(out, "\xe7\xa2\xba\xe5\xae\x9a") == 0);

    assert(rich4_big5_to_utf8("OK 123", out, sizeof(out)));
    assert(strcmp(out, "OK 123") == 0);
    assert(rich4_cp950_to_ucs2('A') == 'A');

    assert(rich4_big5_to_utf8("1.\xa8\xfa\xae\xf8", out, sizeof(out)));
    assert(strcmp(out, "1.\xe5\x8f\x96\xe6\xb6\x88") == 0);

    assert(!rich4_big5_to_utf8("\xa8\xfa", out, 2));
    assert(rich4_big5_to_utf8("", out, sizeof(out)));
    assert(out[0] == '\0');

    printf("test_big5_utf8: ok\n");
    return 0;
}
