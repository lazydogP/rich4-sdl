#include <assert.h>
#include <stdio.h>

#include "resource/byte_reader.h"

int main(void)
{
    const unsigned char bytes[] = { 0x34, 0x12, 0x78, 0x56, 0xab };
    uint16_t value16 = 0;
    uint32_t value32 = 0;

    assert(rich4_byte_reader_le16(bytes, sizeof(bytes), 0, &value16));
    assert(value16 == 0x1234u);
    assert(rich4_byte_reader_le32(bytes, sizeof(bytes), 1, &value32));
    assert(value32 == 0xab567812u);
    assert(!rich4_byte_reader_le16(bytes, sizeof(bytes), 4, &value16));
    assert(!rich4_byte_reader_le32(bytes, sizeof(bytes), 2, &value32));
    assert(!rich4_byte_reader_le16(bytes, SIZE_MAX, SIZE_MAX, &value16));
    assert(!rich4_byte_reader_le16(NULL, 0, 0, &value16));
    puts("bounds-checked byte reader: ok");
    return 0;
}
