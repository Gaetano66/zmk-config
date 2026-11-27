#include <assert.h>
#include "../../src/oled_portrait_mask.h"

static void test_overlong_glyph_fits_before_rotation(void) {
    uint8_t mask[CORNE_OLED_LABEL_BYTES] = {0};
    /* Lit samples at x=0, x=20 and x=39; only the upright x axis should shrink. */
    const uint8_t glyph[] = {0x80, 0, 0x08, 0, 0x01};
    corne_oled_stamp_glyph(mask, 0, 31, 40, 1, 1, glyph, 40);
    assert(mask[0] == 0x80);
    assert(mask[31 * CORNE_OLED_LABEL_STRIDE] == 0x80);
    assert(mask[16 * CORNE_OLED_LABEL_STRIDE] == 0x80);
    assert(mask[20 * CORNE_OLED_LABEL_STRIDE] == 0);
}

int main(void) {
    uint8_t mask[CORNE_OLED_LABEL_BYTES] = {0};
    /* One bright pixel in a 2x2, 4-bpp glyph; the dark samples stay off. */
    const uint8_t glyph[] = {0xf1, 0x07};
    corne_oled_stamp_glyph(mask, 4, 7, 2, 2, 4, glyph, CORNE_OLED_LABEL_SIZE);
    assert(mask[4 * 4 + 3] == 0x80);  /* upright (4, 7) -> OLED (24, 4) */
    assert(mask[5 * 4 + 2] == 0 && mask[5 * 4 + 3] == 0);
    const uint8_t edge[] = {0xf0};
    corne_oled_stamp_glyph(mask, 31, 31, 1, 1, 4, edge, CORNE_OLED_LABEL_SIZE);
    assert(mask[31 * 4] == 0x80);  /* (31,31) -> (0,31) */
    corne_oled_stamp_glyph(mask, -1, -1, 1, 1, 4, edge, CORNE_OLED_LABEL_SIZE);
    assert(mask[0] == 0);
    test_overlong_glyph_fits_before_rotation();
}
