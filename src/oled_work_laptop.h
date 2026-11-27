#pragma once

#include <stdint.h>
#include <string.h>

/* Upright 18x10 laptop art, rotated like the cat for the portrait OLED. */
enum { LAPTOP_WIDTH = 10, LAPTOP_HEIGHT = 18, LAPTOP_STRIDE = (LAPTOP_WIDTH + 7) / 8,
       LAPTOP_BYTES = LAPTOP_STRIDE * LAPTOP_HEIGHT, LAPTOP_STEP_MS = 250 };

static inline void laptop_pixel(uint8_t *mask, unsigned x, unsigned y) {
    unsigned col = LAPTOP_WIDTH - 1 - y;
    mask[x * LAPTOP_STRIDE + col / 8] |= 0x80u >> (col % 8);
}

static inline void laptop_frame(uint8_t mask[LAPTOP_BYTES], unsigned step) {
    memset(mask, 0, LAPTOP_BYTES);
    for (unsigned x = 2; x <= 15; ++x) {
        laptop_pixel(mask, x, 0);
        laptop_pixel(mask, x, 7);
    }
    for (unsigned y = 1; y < 7; ++y) {
        laptop_pixel(mask, 2, y);
        laptop_pixel(mask, 15, y);
    }
    for (unsigned x = 0; x < LAPTOP_HEIGHT; ++x) {
        laptop_pixel(mask, x, 9);
    }
    for (unsigned x = 5; x <= 6; ++x) {
        laptop_pixel(mask, x, 2);
    }
    for (unsigned x = 5; x < 6 + step % 4 * 2; ++x) {
        laptop_pixel(mask, x, 4);
    }
}
