#include <assert.h>
#include "../../src/oled_work_laptop.h"

static unsigned pixel(const uint8_t *mask, unsigned x, unsigned y) {
    unsigned col = LAPTOP_WIDTH - 1 - y;
    return mask[x * LAPTOP_STRIDE + col / 8] & (0x80u >> (col % 8));
}

int main(void) {
    uint8_t first[LAPTOP_BYTES], frame[LAPTOP_BYTES];
    laptop_frame(first, 0);
    laptop_frame(frame, 4);
    assert(memcmp(first, frame, sizeof(first)) == 0);
    for (unsigned step = 0; step < 4; ++step) {
        laptop_frame(frame, step);
        for (unsigned x = 0; x < LAPTOP_HEIGHT; ++x) {
            assert(pixel(frame, x, 9));
            assert(!!pixel(frame, x, 4) == (x == 2 || x == 15 || (x >= 5 && x < 6 + step * 2)));
        }
    }
}
