#pragma once

#include <stdint.h>

enum { CORNE_OLED_LABEL_SIZE = 32, CORNE_OLED_LABEL_STRIDE = CORNE_OLED_LABEL_SIZE / 8,
       CORNE_OLED_LABEL_BYTES = CORNE_OLED_LABEL_SIZE * CORNE_OLED_LABEL_STRIDE };

/* The font's samples are MSB-first, with no padding at glyph row boundaries. */
static inline void corne_oled_stamp_glyph(uint8_t *mask, int x, int y, unsigned width,
                                         unsigned height, unsigned bpp, const uint8_t *glyph,
                                         int line_width) {
    if (bpp != 1 && bpp != 2 && bpp != 4 && bpp != 8) {
        return;
    }
    for (unsigned row = 0; row < height; ++row) {
        for (unsigned col = 0; col < width; ++col) {
            int u = x + (int)col, v = y + (int)row;
            if (u < 0 || v < 0 || v >= CORNE_OLED_LABEL_SIZE) {
                continue;
            }
            unsigned bit = (row * width + col) * bpp;
            /* The alpha MSB selects pixels at or above half opacity on the 1-bit OLED. */
            if (glyph[bit / 8] & (0x80u >> (bit % 8))) {
                /* Merge lit source pixels into destination columns before rotation. */
                if (line_width > CORNE_OLED_LABEL_SIZE) {
                    u = u * CORNE_OLED_LABEL_SIZE / line_width;
                }
                if (u >= CORNE_OLED_LABEL_SIZE) {
                    continue;
                }
                unsigned dx = CORNE_OLED_LABEL_SIZE - 1 - v;
                mask[u * CORNE_OLED_LABEL_STRIDE + dx / 8] |= 0x80u >> (dx % 8);
            }
        }
    }
}
