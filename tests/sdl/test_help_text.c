#include <assert.h>
#include <stdio.h>

#include "platform/sdl1/game_loop_help_data.h"

static void check_empty_lines(void)
{
    static const unsigned char text[] = {'A', 0, 0, 'B', 0};
    size_t offset = 0;
    size_t length = 99;

    assert(help_text_next_line(text, sizeof(text), &offset, &length));
    assert(length == 1 && offset == 2);
    assert(help_text_next_line(text, sizeof(text), &offset, &length));
    assert(length == 0 && offset == 3);
    assert(help_text_next_line(text, sizeof(text), &offset, &length));
    assert(length == 1 && offset == sizeof(text));
    assert(!help_text_next_line(text, sizeof(text), &offset, &length));
}

static void check_explicit_page_break_after_empty_line(void)
{
    static const unsigned char text[] = {
        'A', 0, 0, '@', 0, 'B', 0
    };
    size_t starts[4] = {99, 99, 99, 99};

    assert(help_text_page_starts(text, sizeof(text), starts, 4) == 2);
    assert(starts[0] == 0);
    assert(starts[1] == 5);
}

static void check_automatic_page_break_counts_empty_lines(void)
{
    unsigned char text[64];
    size_t starts[4] = {99, 99, 99, 99};
    size_t offset = 0;
    size_t expected_second_page;
    int line;

    for (line = 0; line < HELP_BODY_LINES + 1; line++) {
        if (line != 4)
            text[offset++] = (unsigned char)('A' + line);
        text[offset++] = 0;
    }
    expected_second_page = HELP_BODY_LINES * 2 - 1;
    assert(help_text_page_starts(text, offset, starts, 4) == 2);
    assert(starts[0] == 0);
    assert(starts[1] == expected_second_page);
}

int main(void)
{
    check_empty_lines();
    check_explicit_page_break_after_empty_line();
    check_automatic_page_break_counts_empty_lines();
    puts("help text blank-line and page parsing: ok");
    return 0;
}
